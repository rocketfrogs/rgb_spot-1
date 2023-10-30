/////////////////////////////////////////////////////////////////////////
//////////////////////// Configuration //////////////////////////////////
/////////////////////////////////////////////////////////////////////////
#define DEVICE_NAME "rgb_spot-1"
#define PIN 12
#define ENABLE_OTA
#define ENABLE_SERIAL

/************************* WiFi Access Point *********************************/

#include <ESP8266WiFi.h>        // Include the Wi-Fi library
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#ifdef ENABLE_OTA
#include <ArduinoOTA.h>
#endif
#include "credentials.h"        // Include Credentials (you need to create that file in the same folder if you cloned it from git)

/*
  Content of "credentials.h" that matters for this section

  // WIFI Credentials

  #define WIFI_SSID        "[REPLACE BY YOUR WIFI SSID (2G)]"     // The SSID (name) of the Wi-Fi network you want to connect to
  #define WIFI_PASSWORD    "[REPLACE BY YOUR WIFI PASSWORD]"      // The password of the Wi-Fi
*/

const char* ssid     = WIFI_SSID;         // The SSID (name) of the Wi-Fi network you want to connect to
const char* password = WIFI_PASSWORD;     // The password of the Wi-Fi

/************************* MQTT Setup *********************************/

#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "credentials.h"

/*
  // MQTT Credentials

  Content of "credentials.h" that matters for this section

  #define AIO_SERVER      "[REPLACE BY YOUR MQTT SERVER IP ADDRESS OR ITS FQDN]"
  #define AIO_SERVERPORT  [REPLACE BY THE PORT NUMBER USED FOR THE MQTT SERVICE ON YOUR MQTT SERVEUR (DEFAULT IS 1883)]       // use 8883 for SSL"
  #define AIO_USERNAME    ""  // USE THIS IF YOU HAVE USERNAME AND PASSWORD ENABLED ON YOUR MQTT SERVER
  #define AIO_KEY         ""  // USE THIS IF YOU HAVE USERNAME AND PASSWORD ENABLED ON YOUR MQTT SERVER
*/

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

// Setup a feeds for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish stat = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/" DEVICE_NAME "/stat"); // current settings

// Setup a feeds for subscribing to changes.
Adafruit_MQTT_Subscribe cmnd = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/" DEVICE_NAME "/cmnd"); // command


// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
void MQTT_connect();



inline void pin(int level) {
  if (level) {
    pinMode(PIN, INPUT_PULLUP);
  } else {
    pinMode(PIN, OUTPUT);
  }
}

inline void header() {
  pin(LOW); delayMicroseconds(13536 / 9 * 13.5);
  pin(HIGH); delayMicroseconds(5040);
}

inline void zero() {
  pin(LOW); delayMicroseconds(560 * 16 / 15);
  pin(HIGH); delayMicroseconds(560 * 16 / 15);
}

inline void one() {
  pin(LOW); delayMicroseconds(560 * 16 / 15);
  pin(HIGH); delayMicroseconds(1674 * 16 / 15);
}

void setup() {

  ///////////////////////////////////////////////////////
  ///////////////////// Start Serial ////////////////////
  ///////////////////////////////////////////////////////

#ifdef ENABLE_SERIAL
  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println('\n');
#endif

  ///////////////////////////////////////////////////////
  //////////////////// Start Wifi ///////////////////////
  ///////////////////////////////////////////////////////

  WiFi.begin(ssid, password);             // Connecting to the network
#ifdef ENABLE_SERIAL
  Serial.print("Connecting to ");
  Serial.print(ssid); Serial.println(" ...");
#endif

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(1000);
#ifdef ENABLE_SERIAL
    Serial.print(++i); Serial.print(' ');
#endif
  }

#ifdef ENABLE_OTA
  ////////////////// Initialize OTA /////////////////////
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(DEVICE_NAME);

#ifdef ENABLE_SERIAL
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
#endif /* ENABLE_SERIAL */

  ArduinoOTA.begin();
#endif /* ENABLE_OTA */

#ifdef ENABLE_SERIAL
  Serial.println('\n');
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer
#endif

  ///////////////////////////////////////////////////////
  ////////////// Suscribing to MQTT topics //////////////
  ///////////////////////////////////////////////////////

  // Setup MQTT subscription feeds.
  mqtt.subscribe(&cmnd);

  ///////////////////////////////////////////////////////
  ////////////////// Initialize PINs ////////////////////
  ///////////////////////////////////////////////////////

  pin(HIGH);
  digitalWrite(PIN, LOW);
}

void send(const char* code) {
  header();
  while (*code) {
    if (*code == '0') zero(); else one();
    code++;
  }
}

char stat_command[20] = "start";

const char* on          = "000000001111111110100010010111011";
const char* shade_green = "000000001111111101100010100111011";
const char* off         = "000000001111111111100010000111011";
const char* C           = "000000001111111100100010110111011";
const char* shade_blue  = "000000001111111100000010111111011";
const char* hour_per_24 = "000000001111111111000010001111011";
const char* S           = "000000001111111111100000000111111";
const char* light_up    = "000000001111111110101000010101111";
const char* light_down  = "000000001111111110010000011011111";
const char* R           = "000000001111111101101000100101111";
const char* G           = "000000001111111110011000011001111";
const char* B           = "000000001111111110110000010011111";
const char* R2          = "000000001111111100110000110011111";
const char* G2          = "000000001111111100011000111001111";
const char* B2          = "000000001111111101111010100001011";
const char* R3          = "000000001111111100010000111011111";
const char* G3          = "000000001111111100111000110001111";
const char* B3          = "000000001111111101011010101001011";
const char* R4          = "000000001111111101000010101111011";
const char* G4          = "000000001111111101001010101101011";
const char* B4          = "000000001111111101010010101011011";

void loop() {
#ifdef ENABLE_OTA
  ArduinoOTA.handle();
#endif

  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  Adafruit_MQTT_Subscribe *subscription;
  if ((subscription = mqtt.readSubscription(0))) {
    if (subscription == &cmnd) {
      const char* button = (char *)subscription->lastread;
      strcpy(stat_command, button);
      if (!strcmp(button, "ON")) {
        send(on);
      } else if (!strcmp(button, "shade_green")) {
        send(shade_green);
      } else if (!strcmp(button, "off")) {
        send(off);
      } else if (!strcmp(button, "C")) {
        send(C);
      } else if (!strcmp(button, "shade_blue")) {
        send(shade_blue);
      } else if (!strcmp(button, "hour_per_24")) {
        send(hour_per_24);
      } else if (!strcmp(button, "S")) {
        send(S);
      } else if (!strcmp(button, "light_up")) {
        send(light_up);
      } else if (!strcmp(button, "light_down")) {
        send(light_down);
      } else if (!strcmp(button, "R")) {
        send(R);
      } else if (!strcmp(button, "G")) {
        send(G);
      } else if (!strcmp(button, "B")) {
        send(B);
      } else if (!strcmp(button, "R2")) {
        send(R2);
      } else if (!strcmp(button, "G2")) {
        send(G2);
      } else if (!strcmp(button, "B2")) {
        send(B2);
      } else if (!strcmp(button, "R3")) {
        send(R3);
      } else if (!strcmp(button, "G3")) {
        send(G3);
      } else if (!strcmp(button, "B3")) {
        send(B3);
      } else if (!strcmp(button, "R4")) {
        send(R4);
      } else if (!strcmp(button, "G4")) {
        send(G4);
      } else if (!strcmp(button, "B4")) {
        send(B4);
      }
    }
  }
  report_status();
}

///////////////////////////////////////////////////////
//////////////// MQTT_connect Function ////////////////
///////////////////////////////////////////////////////

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

#ifdef ENABLE_SERIAL
  Serial.print("Connecting to MQTT... ");
#endif

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
#ifdef ENABLE_SERIAL
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
#endif
    mqtt.disconnect();
    delay(250);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
    }
  }
#ifdef ENABLE_SERIAL
  Serial.println("MQTT Connected!");
#endif
}

///////////////////////////////////////////////////////
////////////// REPORT THE SYSTEM STATUS ///////////////
///////////// ON THE CONSOLE AND ON MQTT //////////////
///////////////////////////////////////////////////////

#define REPORT_STATUS_PERIOD_MS 500
void report_status()
{
  static unsigned long previous_millis = 0;
  if (millis() - previous_millis < REPORT_STATUS_PERIOD_MS) {
    // too early to report the status, abort now
    return;
  }

  /// we repport status and publish to mqtt
  stat.publish(stat_command);

  previous_millis = millis();
}
