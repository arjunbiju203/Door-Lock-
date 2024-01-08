#include <Wire.h>
#include <Adafruit_Fingerprint.h>
#include <Firebase_ESP_Client.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <string.h>
#include <LiquidCrystal_I2C.h>
#include <queue>

#if (defined(_AVR) || defined(ESP8266)) && !defined(AVR_ATmega2560_)
#endif
SoftwareSerial mySerial(D1,D2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

#define login_pin D7   //red wire switch with GND
#define logout_pin D8  //brown wire switch with 3.3v

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID "Arjun"
#define WIFI_PASSWORD "asdfghjk"

#define API_KEY "AIzaSyB5cXfwMZ4K2YQn3KRJ1w2G3IrjfswPh_Q"

#define DATABASE_URL "eric-doorlock-default-rtdb.firebaseio.com"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

WiFiClient wifiClient;
HTTPClient http;

byte last_login;
byte last_logout;

uint8_t id;

unsigned long delayDebounce = 300;  //millis
unsigned long lastTimeButtonStateChange_login = millis();
unsigned long lastTimeButtonStateChange_logout = millis();
unsigned long regDelay = 2000;
unsigned long logOnDelay = 500;

int interval = 5000;

LiquidCrystal_I2C lcd(0x27, 20, 4);
const int relaypin = D0;   //Relay INP Pin
const int buttonPin = 10;  //outswitch pin
int buttonState = 0;

struct Data {
  String date1;
  String time1;
  String log;
  int id1;
};
std::queue<Data> dataQueue;
String globalDate, globalTime, globalLog;
int id_finger;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  lcd.init();  // or lcd.begin(); depending on your library version
  lcd.backlight();
  lcd.print("System Starting");
  delay(1000);
  lcdhomescreen();
  Serial.println("started");
  pinMode(relaypin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  digitalWrite(relaypin, LOW);
  button_init();
  finger_init();
  wifi_init();
  firebase_init();
  rtc_init();
}

// void rtc_init(){
//   http.begin(wifiClient, "http://worldtimeapi.org/api/timezone/Asia/Kolkata"); // Use WiFiClient object here

// }

void rtc_init() {
  configTime(5 * 3600, 30 * 60, "pool.ntp.org", "time.nist.gov");
  Serial.println("\nWaiting for time");
  while (time(nullptr) < 1609372800) {  // Wait until the beginning of 2021 (change as needed)
    Serial.print(".");
    delay(1000);
  }
  Serial.println("Time set");
}



void firebase_init() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void wifi_init() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

void button_init() {
  pinMode(login_pin, INPUT_PULLUP);
  pinMode(logout_pin, INPUT_PULLUP);

  last_login = digitalRead(login_pin);
  last_logout = digitalRead(logout_pin);
}



void finger_init() {
  finger.begin(57600);
  delay(5);
}



void loop() {
  // put your main code here, to run repeatedly:

  readFingerprint();
  outswitch();

  if (millis() - lastTimeButtonStateChange_login >= delayDebounce) {
    byte button_login = digitalRead(login_pin);
    if (button_login != last_login) {  //
      if (button_login == HIGH) {
        //        Serial.println("bu");
        unsigned long nowTime_login = millis();
        unsigned long durationPressed_login = nowTime_login - lastTimeButtonStateChange_login;

        if (durationPressed_login < regDelay && durationPressed_login >= logOnDelay) {
          Serial.println("LOGIN");
          lcd.clear();
          lcd.print("LOGIN");
          lcd.setCursor(0, 1);
          lcd.print("PLACE YOUR FINGER");
          unsigned long currentmillis = millis();
          unsigned long previousmillis = millis();
          while (currentmillis - previousmillis <= interval) {
            currentmillis = millis();
            //int id_finger;
            uint8_t result = getFingerprintID(id_finger);
            if (result == FINGERPRINT_OK) {
              Serial.println("TURN ON RFELAY MODULE!!!");
              relayon();
              String date_, time_;
              if (WiFi.status() != WL_CONNECTED) {
                // If WiFi is not connected, use global variables to store date and time
                get_date_time(globalDate, globalTime);
                date_ = globalDate;
                time_ = globalTime;
              } else {
                // If WiFi is connected, get current date and time
                get_date_time(date_, time_);
              }
              String name_ = get_name(id_finger);
              Serial.print("LOGIN -->");
              Serial.print(" ");
              Serial.print(name_);
              Serial.print(" ");
              Serial.print(date_);
              Serial.print(" ");
              Serial.println(time_);
              lcd.clear();
              lcd.print("LOGIN");
              lcd.setCursor(12, 0);
              lcd.print(time_);
              lcd.setCursor(0, 1);
              lcd.print("NAME : ");
              lcd.print(name_);
              lcd.setCursor(0, 2);
              lcd.print("chipi chipi chapa chapa");
              delay(5000);
              lcdhomescreen();
              set_entry(date_ + "/login/" + name_, time_);
              break;
            } else if (result == FINGERPRINT_NOTFOUND) {
              lcd.clear();
              lcd.print("Please Try again....");
              delay(2000);
              lcdhomescreen();
              break;
            }
            delay(50);
          }
          delay(2000);
          lastTimeButtonStateChange_login = millis();
          last_login = button_login;

          //queue

          if (WiFi.status() != WL_CONNECTED) {
            Data newData;
            Serial.print("No wifi connection");
            newData.date1 = globalDate;
            newData.time1 = globalTime;
            newData.id1 = id_finger;
            newData.log = "LOGIN";
            dataQueue.push(newData);
          } else {
            while (!dataQueue.empty()) {
              Data d = dataQueue.front();
              dataQueue.pop();
              String path = "data/" + String(d.id1);
              Firebase.RTDB.setString(&fbdo, path + "/date", d.date1);
              Firebase.RTDB.setString(&fbdo, path + "/time", d.time1);
              Firebase.RTDB.setInt(&fbdo, path + "/id", d.id1);
              Firebase.RTDB.setString(&fbdo, path + "/LOG", d.log);
              Serial.print("Data has been uploaded to Firebase");
            }
          }
          delay(1000);
          lcdhomescreen();
          loop();
        } else if (durationPressed_login >= regDelay) {
          Serial.println("REG");

          Serial.println("Ready to enroll a fingerprint!");
          lcd.clear();
          lcd.print("FINGPRINT ENROLLMENT");
          lcd.setCursor(0, 1);
          lcd.print("PLACE YOUR FINGER");
          id = get_regid();
          if (id == 0) {  // ID #0 not allowed, try again!
            return;
          }
          Serial.print("Enrolling ID #");
          Serial.println(id);
          uint8_t result;

          //          do{
          //
          //          }while(!getFingerprintEnroll())
          //          while (!getFingerprintEnroll());
          result = getFingerprintEnroll();
          Serial.println(result);
          if (result == 1) {
            set_regid("/DataBase/" + String(id), " ");
          }

          delay(2000);
          lastTimeButtonStateChange_login = millis();
          last_login = button_login;
          lcdhomescreen();
          loop();
        }
      }
      lastTimeButtonStateChange_login = millis();
      last_login = button_login;
    }
  }



  if (millis() - lastTimeButtonStateChange_logout >= delayDebounce) {
    byte button_logout = digitalRead(logout_pin);
    if (button_logout != last_logout) {  //
      if (button_logout == LOW) {
        //        Serial.println("bu");
        unsigned long nowTime_logout = millis();
        unsigned long durationPressed_logout = nowTime_logout - lastTimeButtonStateChange_logout;

        if (durationPressed_logout >= logOnDelay) {
          Serial.println("LOGOUT");
          lcd.clear();
          lcd.print("LOGOUT");
          lcd.setCursor(0, 1);
          lcd.print("PLACE YOUR FINGER");
          unsigned long currentmillis = millis();
          unsigned long previousmillis = millis();
          while (currentmillis - previousmillis <= interval) {
            currentmillis = millis();
            int id_finger;
            uint8_t result = getFingerprintID(id_finger);
            if (result == FINGERPRINT_OK) {
              Serial.println("HAVE A GOOD DAY");
              String date_, time_;
              if (WiFi.status() != WL_CONNECTED) {
                // If WiFi is not connected, use global variables to store date and time
                get_date_time(globalDate, globalTime);
                date_ = globalDate;
                time_ = globalTime;
              } else {
                // If WiFi is connected, get current date and time
                get_date_time(date_, time_);
              }
              String name_ = get_name(id_finger);
              Serial.print("LOGOUT -->");
              Serial.print(" ");
              Serial.print(name_);
              Serial.print(" ");
              Serial.print(date_);
              Serial.print(" ");
              Serial.print(time_);
              lcd.clear();
              lcd.print("LOGOUT");
              lcd.setCursor(12, 0);
              lcd.print(time_);
              lcd.setCursor(0, 1);
              lcd.print("NAME : ");
              lcd.print(name_);
              lcd.setCursor(0, 2);
              lcd.print("DWM bhara kyaa");
              //relayon();
              delay(5000);
              lcdhomescreen();
              set_entry(date_ + "/logout/" + name_, time_);
              break;
            } else if (result == FINGERPRINT_NOTFOUND) {
              lcd.clear();
              lcd.print("Please Try again....");
              delay(2000);
              lcdhomescreen();
              break;
            }
            delay(50);
          }
          delay(2000);
          lastTimeButtonStateChange_logout = millis();
          last_logout = button_logout;

          //queue

          if (WiFi.status() != WL_CONNECTED) {
            Data newData;
            Serial.print("No wifi connection");
            newData.date1 = globalDate;
            newData.time1 = globalTime;
            newData.id1 = id_finger;
            newData.log = "LOGOUT";
            dataQueue.push(newData);
          } else {
            while (!dataQueue.empty()) {
              Data d = dataQueue.front();
              dataQueue.pop();
              String path = "data/" + String(d.id1);
              Firebase.RTDB.setString(&fbdo, path + "/date", d.date1);
              Firebase.RTDB.setString(&fbdo, path + "/time", d.time1);
              Firebase.RTDB.setInt(&fbdo, path + "/id", d.id1);
              Firebase.RTDB.setString(&fbdo, path + "/LOG", d.log);
              Serial.print("Data has been uploaded to Firebase");
            }
          }
          lcdhomescreen();
          loop();
        }
      }
      lastTimeButtonStateChange_logout = millis();
      last_logout = button_logout;
    }
  }
}


// void get_date_time(String &date_, String &time_){
//   int httpCode = http.GET();

//   if (httpCode > 0) {
//     String payload = http.getString();

//     DynamicJsonDocument doc(1024);
//     deserializeJson(doc, payload);

//     String datetime = doc["datetime"].as<String>();
//     date_ = datetime.substring(0, 10);
//     time_ = datetime.substring(11, 19);

//  //    Serial.print("Date: ");s
//  //    Serial.println(date);
//  //    Serial.print("Time: ");
//  //    Serial.println(time);
//   }
//   else {
//     Serial.println("Error in HTTP request");
//   }
//   http.end();
// }

void get_date_time(String& date, String& timeStr) {
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);

  date = String(p_tm->tm_year + 1900);  // Years since 1900
  date += "-";
  date += String(p_tm->tm_mon + 1);  // Months are zero-based
  date += "-";
  date += String(p_tm->tm_mday);

  timeStr = String(p_tm->tm_hour);
  timeStr += ":";
  timeStr += String(p_tm->tm_min);
  timeStr += ":";
  timeStr += String(p_tm->tm_sec);
}

void readFingerprint() {
  int id_finger;
  uint8_t result = getFingerprintID(id_finger);
  if (result == FINGERPRINT_OK) {
    Serial.println("TURN ON RELAY MODULE!!!");
    relayon();
    Serial.println(get_name(id_finger));
    lcd.clear();
    lcd.print(get_name(id_finger));
    delay(2000);
    lcdhomescreen();
  } else if (result == FINGERPRINT_NOTFOUND) {
    lcd.clear();
    lcd.print("Please Try again.....");
    delay(2000);
    lcdhomescreen();
  }
  delay(50);
}


uint8_t getFingerprintID(int& id_finger) {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  // found a match!
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  id_finger = finger.fingerID;
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);

  return p;
}


uint8_t readnumber(void) {
  uint8_t num = 0;

  while (num == 0) {
    while (!Serial.available())
      ;
    num = Serial.parseInt();
  }
  return num;
}


uint8_t getFingerprintEnroll() {

  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #");
  Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.println(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID ");
  Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");
  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID ");
  Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  return true;
}


String get_name(int id) {
  if (Firebase.RTDB.getString(&fbdo, "/DataBase/" + String(id))) {
    if (fbdo.dataType() == "string") {
      String name = fbdo.stringData();
      //      Serial.println("Name found: " + name);
      return name;
    } else {
      Serial.println("No string data found for the key");
    }
  } else {
    Serial.println("Failed to retrieve data: " + fbdo.errorReason());
  }
  return "Unknown";
}


int get_regid() {
  if (Firebase.RTDB.get(&fbdo, "/DataBase")) {
    if (fbdo.dataType() == "array") {
      FirebaseJsonArray* jsonArray = fbdo.jsonArrayPtr();
      int arrayLength = jsonArray->size();
      return arrayLength;
      //      Serial.println("Length of array in DataBase/: " + String(arrayLength));
    } else {
      Serial.println("Data type is not an array. Data type is: " + fbdo.dataType());
    }
  } else {
    Serial.println("Failed to retrieve data: " + fbdo.errorReason());
  }
  return -1;
}


void set_regid(String url, String data_string) {
  Firebase.RTDB.setString(&fbdo, url, data_string);
}

void set_entry(String url, String data_string) {
  Firebase.RTDB.setString(&fbdo, url, data_string);
}

void relayon() {
  digitalWrite(relaypin, HIGH);
  delay(2000);
  digitalWrite(relaypin, LOW);
  Serial.print("Turn of Relay Module");
}

void lcdhomescreen() {
  lcd.clear();
  lcd.setCursor(15, 0);
  lcd.print("LOGIN");
  lcd.setCursor(0, 1);
  lcd.print("Welcome To");
  lcd.setCursor(0, 2);
  lcd.print("ERIC ROBOTICS");
  lcd.setCursor(14, 3);
  lcd.print("LOGOUT");
}

void outswitch() {
  buttonState = digitalRead(buttonPin);
  if (buttonState == HIGH) {
    Serial.println("Button Pressed");
    digitalWrite(relaypin, HIGH);
    while (digitalRead(buttonPin) == HIGH) {
      delay(10);  // Small delay to prevent bouncing issues
    }
    Serial.println("Button Released");
    digitalWrite(relaypin, LOW);
  }
  delay(1000);
}
// void queue_test()
// {
//   if (WiFi.status() != WL_CONNECTED)
//   {
//     Data newData;
//     newData.name1 = name_ ;
//     newData.date1 = date_;
//     newData.time1 = time_;
//     newData.id1 = id_finger;
//     dataQueue.push(newData);
//   }
//   else
//   {
//     while (!dataQueue.empty())
//     {
//       Data d = dataQueue.front();
//       dataQueue.pop();
//       String path = "data/" + String(d.id1);
//       Firebase.RTDB.setString(&fbdo, path + "/name", d.name1);
//       Firebase.RTDB.setString(&fbdo, path + "/date", d.date1);
//       Firebase.RTDB.setString(&fbdo, path + "/time", d.time1);
//       Firebase.RTDB.setInt(&fbdo, path + "/id", d.id1);
//     }
//   }
//   delay(1000);
// }