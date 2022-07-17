#include <WiFi.h>
#include <WiFiUdp.h>

#include <ESPmDNS.h>
#include <ArduinoOTA.h>

#include <WebServer.h>
#include "Arduino_JSON.h"

#include <ESP32Servo.h>
#include <Ticker.h>

#include <secret.h>

Servo myservo;
WebServer server(80);
Ticker timer_autolock;

WiFiClient wifi;

const char* HOSTNAME = "esp32_door";

const int button1 = 32;
const int button2 = 33;
const int mosfet = 12;
const int doorSensor = 14;
const int morter_pwm = 13;
const int angleUnlock = 85;
const int angleLock = 180;
const int autoLockSecond = 10;
const bool enableautoLock = true;

bool timerOn = false;

int doorStatus;
bool doorLockStatus;
int button1Status;
int button2Status;

void setup() {
    WiFisetup();
    OTAsetup();
    HTTPsetup();

    myservo.attach(morter_pwm);
    pinMode(button1, INPUT_PULLUP);
    pinMode(button2, INPUT_PULLUP);
    pinMode(mosfet, OUTPUT);
    pinMode(doorSensor, INPUT_PULLUP);

    unlock();
}

void loop() {
    ArduinoOTA.handle();
    server.handleClient();

    button1Status = !digitalRead(button1);
    button2Status = !digitalRead(button2);
    doorStatus = !digitalRead(doorSensor);

    if (button1Status == HIGH) {
        unlock();
    }

    if (button2Status == HIGH) {
        lock();
    }

    if (doorStatus == LOW && doorLockStatus == true) {
        // Unexpected behavior, Picking or Used Key.
        doorLockStatus = false;
    }

    if (enableautoLock && doorStatus == HIGH && doorLockStatus == false) {
        if (!timerOn) {
            timer_autolock.once(autoLockSecond, autolock);
            timerOn = true;
        }
        if (button2Status == 1) {
            autolock();
        }
    }

    reconnect();
}

void WiFisetup() {
    Serial.begin(115200);
    Serial.println("Booting ESP32.\n Connecting Network...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, WIFI_PASSWORD);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("Connection Failed! Rebooting...");
        delay(5000);
        ESP.restart();
    }
}


void OTAsetup() {
    ArduinoOTA.setHostname(HOSTNAME);

    ArduinoOTA
        .onStart([]() {
                String type;
                if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
                else
                type = "filesystem";

                Serial.println("Start updating " + type);
                })
        .onEnd([]() {
                Serial.println("\nEnd");
                })
        .onProgress([](unsigned int progress, unsigned int total) {
                Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
                })
        .onError([](ota_error_t error) {
                Serial.printf("Error[%u]: ", error);
                if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
                else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
                else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
                else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
                else if (error == OTA_END_ERROR) Serial.println("End Failed");
                });

    ArduinoOTA.begin();
}

void HTTPsetup() {
    server.on("/lock", apilock);
    server.on("/unlock", apiunlock);
    server.begin();
}

void reconnect() {
    if ( WiFi.status() == WL_DISCONNECTED ) {
        WiFisetup();
    }
}


void unlock() {
    digitalWrite(mosfet, HIGH);
    myservo.write(angleUnlock);
    doorLockStatus = false;
    delay(1000);
    myservo.write(angleUnlock);
    digitalWrite(mosfet, LOW);
    Serial.println("Unlocked");
}

void lock() {
    if (doorStatus == 1) {
        digitalWrite(mosfet, HIGH);
        myservo.write(angleUnlock);
        myservo.write(angleLock);
        doorLockStatus = true;
        delay(1000);
        myservo.write(angleUnlock);
        digitalWrite(mosfet, LOW);
        Serial.println("Locked");
    }
}

void apiunlock() {
    unlock();
    server.send(200, "application/json", "{\"status\": \"unlocked\"}");
}

void apilock() {
    lock();
    server.send(200, "application/json", "{\"status\": \"locked\"}");
}

void autolock() {
    lock();
    timer_autolock.detach();
    timerOn = false;
}

