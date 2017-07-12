/*
 * Storm Jar
 * Get weather forecast, then simulate in a jar.
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
extern "C" {
    #include "user_interface.h"
}
extern "C" {
    #include "gpio.h"
}
#include <WiFiClientSecure.h>
#include <aJSON.h>
#include "config.h"


// Create Neopixel object
Adafruit_NeoPixel ring = Adafruit_NeoPixel(LED_COUNT, PIN_LED, NEO_GRB + NEO_KHZ800);

void setup()
{
    // initialize PUMP/FOG pins as output and set to off
    pinMode(PIN_PUMP, OUTPUT);
    pinMode(PIN_FOG, OUTPUT);
    pinMode(PIN_SWITCH, INPUT);
    digitalWrite(PIN_FOG, LOW);
    digitalWrite(PIN_PUMP, LOW);
    digitalWrite(PIN_LED, LOW);

    Serial.begin(115200);
    Serial.println();

    WiFi.begin(ussid, pwd);

    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println();

    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
}

void clear() {
    for (int j = 0; j < LED_COUNT; j++) {
        ring.setPixelColor(j, 127, 32, 0, BRIGHTNESS);
        ring.show();
    }

    delay(PLAYTIME);
}

void cloud(int t_zero) {
    digitalWrite(PIN_FOG, HIGH);

    while (millis() < t_zero + PLAYTIME) {
        FadeInOut(0xff, 0xff, 0xff, 20); // white
    }

    delay(PLAYTIME);
}

void rain(int t_zero) {
    digitalWrite(PIN_PUMP, HIGH);

    while (millis() < t_zero + PLAYTIME) {
        theaterChase(0, 0, 0xff, 200); // blue
    }
}

void snow(int t_zero) {
    digitalWrite(PIN_FOG, HIGH);

    while (millis() < t_zero + PLAYTIME) {
        Sparkle(0x10, 0x10, 0x10, 100); // white
    }
}

void Sparkle(byte red, byte green, byte blue, int SpeedDelay)
{
    int Pixel = random(LED_COUNT);
    setPixel(Pixel,red,green,blue);
    ring.show();

    delay(SpeedDelay);

    setPixel(Pixel, 0, 0, 0);
}

void theaterChase(byte red, byte green, byte blue, int SpeedDelay) {
    for (int q=0; q < 3; q++) {
        for (int i=0; i < LED_COUNT; i=i+3) {
            setPixel(i+q, red, green, blue); // turn every third pixel on
        }
        ring.show();
        delay(SpeedDelay);

        for (int i=0; i < LED_COUNT; i=i+3) {
            setPixel(i+q, 0,0,0); // turn every third pixel off
        }
    }
}

void setPixel(int Pixel, byte red, byte green, byte blue)
{
    ring.setPixelColor(Pixel, ring.Color(red, green, blue));
}

void FadeInOut(byte red, byte green, byte blue, int SpeedDelay){
    float r, g, b;

    for(int k = 0; k < 256; k=k+1) {
        r = (k/256.0)*red;
        g = (k/256.0)*green;
        b = (k/256.0)*blue;
        for(int i = 0; i < LED_COUNT; i++) {
            setPixel(i, r, g, b);
        }
        ring.show();
        delay(SpeedDelay);
    }

    for(int k = 255; k >= 0; k=k-2) {
        r = (k/256.0)*red;
        g = (k/256.0)*green;
        b = (k/256.0)*blue;
        for(int i = 0; i < LED_COUNT; i++) {
            setPixel(i, r, g, b);
        }
        ring.show();
        delay(SpeedDelay);
    }
}

void sleep() {
    Serial.println("Going to sleep");
    wifi_station_disconnect();
    wifi_set_opmode(NULL_MODE);
    wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
    gpio_pin_wakeup_enable(GPIO_ID_PIN(2), GPIO_PIN_INTR_HILEVEL);
    wifi_fpm_open();
    delay(100);
    wifi_fpm_set_wakeup_cb(wakeup);
    wifi_fpm_do_sleep(0xFFFFFFF);
}

void wakeup() {
    wifi_fpm_close();
    wifi_set_opmode(STATION_MODE);
    wifi_station_connect();
    Serial.println("Woke up!");
}

void loop()
{
    // RESET all outputs
    ring.begin();
    for (int i = 0; i < LED_COUNT; i++) {
        ring.setPixelColor(i, 0, 0, 0, 0);
        ring.show();
    }
    digitalWrite(PIN_FOG, LOW);
    delay(1000);
    digitalWrite(PIN_FOG, HIGH);
    delay(1000);
    digitalWrite(PIN_FOG, LOW);
    digitalWrite(PIN_PUMP, LOW);

    // Use WiFiClientSecure class to create TLS connection
    WiFiClientSecure client;
    Serial.print("connecting to ");
    Serial.println(host);
    if (!client.connect(host, port)) {
        Serial.println("connection failed");
    return;
    }
    String url = "/forecast/" + secret + "/" + lat + "," + lon + "?exclude=minutely,hourly,daily,flags&units=si";

    client.print(String("GET ") +
                 url +
                 " HTTP/1.1\r\n" +
                 "Host: " +
                 host + "\r\n"
                 + "User-Agent: BuildFailureDetectorESP8266\r\n" +
                 "Connection: close\r\n\r\n");


    String json_string = "";
    boolean httpBody = false;
    while (client.connected()) {
        String line = client.readStringUntil('\r');
        if (!httpBody && line.charAt(1) == '{') {
            httpBody = true;
        }
        if (httpBody) {
            json_string += line;
        }
    }
    char *json_char = new char[json_string.length() + 1];
    strcpy(json_char, json_string.c_str());
    aJsonObject* root = aJson.parse(json_char);
    delete [] json_char;
    aJsonObject* currently = aJson.getObjectItem(root, "currently");
    aJsonObject* icon = aJson.getObjectItem(currently, "icon");
    String weather = icon->valuestring;

    Serial.println(weather);
    Serial.println("closing connection");

    if (weather == "rain" || weather == "sleet") {
        rain(millis());
        delay(PLAYTIME);
    } else if (weather == "cloudy" || weather == "fog" || weather == "partly-cloudy-day" || weather == "partly-cloudy-night") {
        cloud(millis());
        delay(PLAYTIME);
    } else if (weather == "snow") {
        snow(millis());
        delay(PLAYTIME);
    } else {
        clear();
        delay(PLAYTIME);
    }

    // if (digitalRead(PIN_SWITCH) == 0) {
    //     sleep();
    // }
}
