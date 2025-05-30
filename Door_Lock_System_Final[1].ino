#include <Adafruit_Fingerprint.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <avr/wdt.h>


#define LED_PIN 6
#define BUZZER_PIN 9
#define SERVO1_PIN 11
#define SERVO2_PIN 12
#define RESTART_BUTTON_PIN 5
#define NAME_LENGTH 16


SoftwareSerial fingerSerial(2, 3);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);
LiquidCrystal_I2C lcd(0x27, 16, 2);


Servo servo1;
Servo servo2;


int failCount = 0;
bool doorOpened = false;


void lockServos() {
 servo1.write(0);
 servo2.write(0);
}


void unlockServos() {
 servo1.write(90);
 servo2.write(90);
}


uint8_t getNextAvailableID() {
 for (uint8_t id = 1; id < 127; id++) {
   if (finger.loadModel(id) != FINGERPRINT_OK) {
     return id;
   }
 }
 return 0;
}


void storeName(uint8_t id, const char* name) {
 int addr = id * NAME_LENGTH;
 for (int i = 0; i < NAME_LENGTH; i++) {
   EEPROM.write(addr + i, i < strlen(name) ? name[i] : 0);
 }
}


void getName(uint8_t id, char* name) {
 int addr = id * NAME_LENGTH;
 for (int i = 0; i < NAME_LENGTH; i++) {
   name[i] = EEPROM.read(addr + i);
 }
 name[NAME_LENGTH - 1] = '\0';
}


void restartSystem() {
 wdt_enable(WDTO_15MS);
 while (1);
}


void setup() {
 pinMode(LED_PIN, OUTPUT);
 pinMode(BUZZER_PIN, OUTPUT);
 pinMode(RESTART_BUTTON_PIN, INPUT_PULLUP);


 servo1.attach(SERVO1_PIN);
 servo2.attach(SERVO2_PIN);
 lockServos();


 lcd.init();
 lcd.backlight();


 Serial.begin(9600);
 finger.begin(57600);


 if (finger.verifyPassword()) {
   lcd.print("Sensor Ready");
 } else {
   lcd.print("Sensor Error");
   while (1);
 }


 delay(2000);
 lcd.clear();
 enrollFingerprint();  // Initial enrollment only
}


void loop() {
 scanFingerprint();


 // Restart only allowed after successful door open
 if (doorOpened && digitalRead(RESTART_BUTTON_PIN) == LOW) {
   lcd.clear();
   lcd.print("Restarting...");
   delay(1000);
   lcd.clear();
   doorOpened = false;
   enrollFingerprint();
 }
}


void enrollFingerprint() {
 uint8_t id = getNextAvailableID();
 if (id == 0) {
   lcd.print("No IDs left");
   delay(2000);
   return;
 }


 lcd.print("Place Finger");
 lcd.setCursor(0, 1);  
 lcd.print("For Enrollment");
 while (finger.getImage() != FINGERPRINT_OK);
 if (finger.image2Tz(1) != FINGERPRINT_OK) return;


 lcd.clear();
 lcd.print("Remove Finger");
 delay(2000);
 while (finger.getImage() != FINGERPRINT_NOFINGER);


 lcd.clear();
 lcd.print("Scan Again");
 while (finger.getImage() != FINGERPRINT_OK);
 if (finger.image2Tz(2) != FINGERPRINT_OK) return;


 if (finger.createModel() != FINGERPRINT_OK) return;
 if (finger.storeModel(id) != FINGERPRINT_OK) return;


 lcd.clear();
 lcd.print("Enter Name:");
 Serial.println("Enter name for ID " + String(id) + ":");


 while (Serial.available() == 0);
 String name = Serial.readStringUntil('\n');
 name.trim();
 storeName(id, name.c_str());


 lcd.clear();
 lcd.print("Enrolled!");
 delay(2000);
 lcd.clear();
 failCount = 0;
}


void scanFingerprint() {
 lcd.print("Place Finger");
 lcd.setCursor(0, 1);  
 lcd.print("To open Door");
 while (finger.getImage() != FINGERPRINT_OK);
 if (finger.image2Tz() != FINGERPRINT_OK) return;


 if (finger.fingerSearch() != FINGERPRINT_OK) {
   lcd.clear();
   lcd.print("Try Again");
   digitalWrite(LED_PIN, HIGH);
   tone(BUZZER_PIN, 2000, 1000);
   delay(1000);
   digitalWrite(LED_PIN, LOW);
   lcd.clear();


   failCount++;
   if (failCount >= 3) {
     lcd.print("Too Many Fails");
     delay(1500);
     restartSystem();
   }


   return;
 }


 char name[NAME_LENGTH];
 getName(finger.fingerID, name);
 lcd.clear();
 lcd.print("Welcome:");
 lcd.setCursor(0, 1);
 lcd.print(name);


 tone(BUZZER_PIN, 1000, 200);
 unlockServos();
 delay(4500);
 lockServos();
 lcd.clear();
 failCount = 0;
 doorOpened = true; // Allow restart button after this
}



