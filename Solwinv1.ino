#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Adafruit_INA219.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "ChargerClasses.hpp"

#define SolarSwitch 18  // GPIO 16
#define WindSwitch 19  // GPIO 17
#define BAT 34
#define PID 5
#define MAINSW 23


int Num=1;
//wifi data
const char* ssid = "SLT-Fiber-2.4G";
const char* password = "12341234d";

// Firebase project credentials
#define FIREBASE_HOST "https://solwinv1-68380-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "2j9zfBbU7XJJA6d5qMgO0E6gcr0vCFfoZOFSdK7u"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1    // Reset pin (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // For vSolar and iSolar
Adafruit_SSD1306 display2(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // For vWind and iWind

// Current sensor
Adafruit_INA219 INA_1(0x40);
Adafruit_INA219 INA_2(0x44);
 

// Charge control

SimplePID voltagePID;
bool charging = true;

// Define Firebase objects
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

void setup() {


  Serial.begin(115200);
  //Serial.begin(115200);
  pinMode(PID, OUTPUT); // Set NPN_PIN as output
  pinMode(BAT,INPUT);
  pinMode(WindSwitch,INPUT);
  pinMode(SolarSwitch,INPUT);
  pinMode(MAINSW,INPUT);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {

    Serial.print(".");
  }
 
  Serial.println();
  Serial.println("Connected to WiFi.");

  // Configure Firebase
  firebaseConfig.host = FIREBASE_HOST;
  firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;

  // Initialize Firebase
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);

   // Initialize displays
  if (!display1.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed for display1"));
    for (;;);
  }
  if (!display2.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed for display2"));
    for (;;);
  }

  
  // Current sensor
  INA_1.begin();
  INA_2.begin();

  // Charging control
  voltagePID.setParams(0, 0.2e-3, 0, 0, 1); // PID parameters

   digitalWrite(MAINSW, HIGH); 

}


void loop() {
    float iWind = INA_1.getCurrent_mA();
    float iSolar = INA_2.getCurrent_mA();

    
  // Voltage measurement
  float vWind = INA_1.getBusVoltage_V() - INA_1.getShuntVoltage_mV() / 1000;
  float vSolar = INA_2.getBusVoltage_V() - INA_2.getShuntVoltage_mV() / 1000;

    // Voltage at A0 (use proper resistor divider ratio, assuming (5 + 1) / 1)
  float vA0 = analogRead(BAT);
  float vSrc = (vA0 * 5.0) / 1023.0;  // 5V reference, adjusted for divider ratio

    // Charge control
  float targetCurrent = 500;  // Target current in mA
  float u = voltagePID.evalu(iSolar, targetCurrent);
  float duty = u;

  
  // Constrain duty cycle between 0 and 1
  duty = constrain(duty, 0.0, 1.0);

  // Set the duty cycle using analogWrite
  analogWrite(PID, duty * 255);  // Convert to 8-bit PWM (0-255)

    // Format to 2 decimal places when displaying
  String solarVoltageStr = String(vSolar, 2);
  String solarCurrentStr = String(iSolar, 2);
  String windVoltageStr = String(vWind, 2);
  String windCurrentStr = String(iWind, 2);
  String sourceVoltageStr = String(vSrc, 2);

  display1.clearDisplay();
  display1.setTextSize(1);      // Normal 1:1 pixel scale
  display1.setTextColor(SSD1306_WHITE); // Draw white text
  display1.setCursor(0, 0);     // Start at top-left corner
  display1.print(F("Solar Voltage: "));



  if (Firebase.setFloat(firebaseData, "/Bat", sourceVoltageStr.toFloat())) {
  Serial.println("Float data updated successfully.");
}
else {
  Serial.println("Failed to update float data.");
  Serial.println("REASON: " + firebaseData.errorReason());
}


         //firebase
      if (Firebase.setFloat(firebaseData, "/SolVolt", vSolar)) {
      Serial.println("Data sent successfully.");
      }
    else {
      Serial.println("Failed to send data.");
      Serial.println("REASON: " + firebaseData.errorReason());
    }

     //firebase
      if (Firebase.setString(firebaseData, "/SolCurrent", iSolar)) {
      Serial.println("Data sent successfully.");
      }
    else {
      Serial.println("Failed to send data.");
      Serial.println("REASON: " + firebaseData.errorReason());
    }


          if (Firebase.setFloat(firebaseData, "/WindVolt", vWind)) {
      Serial.println("Data sent successfully.");
      }
    else {
      Serial.println("Failed to send data.");
      Serial.println("REASON: " + firebaseData.errorReason());
    }

     //firebase
      if (Firebase.setString(firebaseData, "/WindCurrent", iWind)) {
      Serial.println("Data sent successfully.");
      }
    else {
      Serial.println("Failed to send data.");
      Serial.println("REASON: " + firebaseData.errorReason());
    }
  // Print the result to the serial port
  Serial.print("Target Current: ");
  Serial.print(targetCurrent);
  Serial.print(" mA | Solar Current: ");
  Serial.print(iSolar);
  Serial.print(" mA | Source Voltage: ");
  Serial.print(vSrc);
  Serial.print(" V | Solar Voltage: ");
  Serial.print(vSolar);
  Serial.print(" V | Duty Cycle: ");
  Serial.print(duty * 100);
  Serial.println("%");

      // Read "SwitchState" from Firebase
    if (Firebase.getString(firebaseData, "/solarControl")) {
        String switchState = firebaseData.stringData();
        Serial.print("Switch State: ");
        Serial.println(switchState);

        // Control the switches based on "turn on" or "turn off"
        if (switchState == "turn on") {
            digitalWrite(SolarSwitch , HIGH);  // Turn on the system or switch
            Serial.println("System turned ON");
        } else if (switchState == "turn off") {
            digitalWrite(SolarSwitch, LOW);   // Turn off the system or switch
            Serial.println("System turned OFF");
        }
    } else {
        Serial.println("Failed to get switch state from Firebase.");
        Serial.println("REASON: " + firebaseData.errorReason());
    }

        if (Firebase.getString(firebaseData, "/windControl")) {
        String switchState = firebaseData.stringData();
        Serial.print("Switch State: ");
        Serial.println(switchState);

        // Control the switches based on "turn on" or "turn off"
        if (switchState == "turn on") {
            digitalWrite( WindSwitch, HIGH);  // Turn on the system or switch
            Serial.println("System turned ON");
        } else if (switchState == "turn off") {
            digitalWrite(WindSwitch, LOW);   // Turn off the system or switch
            Serial.println("System turned OFF");
        }
    } else {
        Serial.println("Failed to get switch state from Firebase.");
        Serial.println("REASON: " + firebaseData.errorReason());
    }

  delay(1000);

}
