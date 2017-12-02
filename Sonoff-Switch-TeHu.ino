/*
   Sonoff-Touch-TeHu
   Firmware to use a Sonoff TH10/16 device as a remote relay and temp/humidity sensor with MQTT and WiFi capabilities.

   Supports OTA update
   Mats Ekberg (C) 2017 GNU GPL v3

   Supports sensors (has to be recompiled if change):
     DHT 11
     DHT 21  (AM2301)  This is the default sensor type
     DHT 22  (AM2302)

   Runs on this harware:
   https://www.itead.cc/wiki/Sonoff_TH_10/16

   Uses these libraries:
   https://github.com/adafruit/DHT-sensor-library
   https://github.com/adafruit/Adafruit_Sensor

   Flashed via USB/OTA in Arduino IDE with these parameters:
   Board:       Generic ESP8266 Module
   Flash size:  1M (64K SPIFFS)

*/

// DO EDIT
#define CONFIG_VERSION "THSW001"
#define TH10
// END - DO EDIT


// DO NOT CHANGE
#include "sensorlibs.h"
#include "support/wifi-manager.h"
#include "support/mqtt-support.h"

#include "topics.h"
#include "support/wifi-manager.cpp"
#include "support/mqtt-support.cpp"
// END - DO NOT CHANGE


#define DHTPIN 14
//#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define DHTTYPE DHT21   // DHT 21 (AM2301)
float humid = NAN;
float temp = NAN;
boolean sendSensors = false;
DHT dht(DHTPIN, DHTTYPE);



//
// MQTT message arrived, decode
//
void mqttCallbackHandle(char* topic, byte* payload, unsigned int length) {
  Serial.print(F("MQTT sub: "));
  Serial.println(topic);
}

//
// Handle short touch
//
void shortPress() {
  desiredRelayState = !desiredRelayState; //Toggle relay state.
  sendGroupEventTopic = false;
  sendEvent = true;
  noOfConfigTouches = 0;
}

//
// Handle long touch
//
void longPress() {
  desiredRelayState = !desiredRelayState; //Toggle relay state.
  sendGroupEventTopic = true;
  sendEvent = true;
  noOfConfigTouches = 0;
}

//
// Handle looong config touch
//
void configWifiPress() {
  noOfConfigTouches++;
  if (noOfConfigTouches >= CONFIG_TOUCHES_COUNT)
    configWifi = true;
}


//
// This is executed on touch
//
void buttonChangeCallback() {
  if (digitalRead(0) == 1) {

    // Button has been released, trigger one of the two possible options.
    if (millis() - millisSinceChange > CONFIG_WIFI_PRESS_MS) {
      configWifiPress();
    }
    else if (millis() - millisSinceChange > LONG_PRESS_MS) {
      longPress();
    }
    else if (millis() - millisSinceChange > SHORT_PRESS_MS) {
      shortPress();
    }
    else {
      //Too short to register as a press
    }
  }
  else {
    //Just been pressed - do nothing until released.
  }
  millisSinceChange = millis();
}

DynamicJsonBuffer jsonBuffer(250);


//
// This routine handles state changes and MQTT publishing
//
void handleStatusChange() {

  // publish relay state, pong, event and status messages
  mqttPublish();

  if (sendSensors)
  {
    if (isnan(temp) || isnan(humid))
    {
      Serial.println("No sensor data");
    }
    else
    {
      JsonObject& json = jsonBuffer.createObject();
      char buf1[10],buf2[10];
      dtostrf(temp + 0.05, 5, 1, buf1);
      dtostrf(humid + 0.05, 5, 1, buf2);
      json["temp"] = buf1;
      json["humid"] = buf2;
      String jsonStr;
      json.printTo(jsonStr);
      mqttPublishMessage(sensorTopic.c_str(), jsonStr.c_str());
    }
    sendSensors = false;
  }
}

//
// callback to create custom topics
//
void mqttCallbackCreateTopics() {
  sensorTopic = String(F("sensor/")) + custom_unit_id.getValue() + String(F("/value"));

  // pointer of topics
  //subscribedTopics[0] = &matrixActionSTopic;
  noSubscribedTopics = 0;
}


//
////////// SETUP //////////
//
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("Initialising"));
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, HIGH); //LED off.

  // setup wifi
  wifiSetup(CONFIG_VERSION, false);

  // setup mqtt
  mqttSetup();

  dht.begin();

  // Enable interrupt for button press
  Serial.println(F("Enabling touch switch interrupt"));
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonChangeCallback, CHANGE);
}


//
////////// LOOP //////////
//
void loop() {

  // handle wifi
  wifiLoop();

  // handle mqtt
  mqttLoop();

  // Check MQTT connection
  if (millis() - lastMQTTCheck >= MQTT_CHECK_MS) {
    lastMQTTCheck = millis();

    uptime += MQTT_CHECK_MS / 1000;
    mqttCheckConnection();

    Serial.print(F("sensor read... "));
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float f1 = dht.readHumidity();
    float f2 = dht.readHumidity();
    if (!isnan(f1)) humid = f1;
    if (!isnan(f2) && !isnan(humid)) humid = (humid + f2) / 2.0;
    // Read temperature as Celsius (the default)
    f1 = dht.readTemperature();
    f2 = dht.readTemperature();
    if (!isnan(f1)) temp = f1;
    if (!isnan(f2) && !isnan(temp)) temp = (temp + f2) / 2.0;
    Serial.println(F("done"));

    sendSensors = true;
  }

  // Handle any state change and MQTT publishing
  handleStatusChange();


  delay(50);
}
