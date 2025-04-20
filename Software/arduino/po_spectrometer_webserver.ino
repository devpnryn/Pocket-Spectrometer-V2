#include <M5StickCPlus.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "html_content.h"
#include "AS7341.h"

// WiFi credentials - CHANGE THESE
const char *ssid = "wifi";
const char *password = "password";

// Create web server object
WebServer server(80);

// Create AS7341 sensor object
AS7341 sensor;

// Global variables for gain and integration time settings
uint8_t gainCodes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
float gainFactors[] = {0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
int gainIndex = 4; // Starting with gain factor 8 (index 4)
uint8_t atimeSettings[] = {9, 19, 29, 39, 49, 59};
int atimeIndex = 2;      // Starting with 29 (default value)
int wavelengthIndex = 2; // Default to blue(index 2)
String wavelengthNames[] = {"Violet", "Vio-Blue", "Blue", "Cyan", "Green", "Yellow", "Orange", "Red", "NIR"};
int wavelengthValues[] = {415, 445, 480, 515, 555, 590, 630, 680, 940}; // Center wavelengths in nm

// Spectral data storage
uint16_t spectralData[9]; // F1-F8 + NIR

// Flag to indicate if a new measurement was taken
bool newMeasurementTaken = false;

// Function to update sensor settings
void updateSettings()
{
    sensor.setATime(atimeSettings[atimeIndex]);
    sensor.setGain(gainCodes[gainIndex]);
}

// Function to take a measurement with mock data
void takeMeasurement()
{
    // Generate mock data based on gain and integration time
    float gainFactor = gainFactors[gainIndex];
    float integrationFactor = (atimeSettings[atimeIndex] + 1) / 29.0;

    // Base values for different wavelengths
    uint16_t baseValues[] = {120, 240, 350, 280, 190, 220, 310, 180, 90};

    // Generate realistic-looking mock data
    for (int i = 0; i < 9; i++)
    {
        // Add some random variation (±20%)
        float randomFactor = 0.8 + (random(40) / 100.0);

        // Calculate final value with all factors
        spectralData[i] = baseValues[i] * gainFactor * integrationFactor * randomFactor;
    }

    // Set flag to indicate new measurement
    newMeasurementTaken = true;
}

void handleRoot()
{
    server.send(200, "text/html", index_html);
}

void handleData()
{
    String json = "{";
    json += "\"wavelengths\": [415, 445, 480, 515, 555, 590, 630, 680, 940],";
    json += "\"names\": [\"Violet\", \"Vio-Blue\", \"Blue\", \"Cyan\", \"Green\", \"Yellow\", \"Orange\", \"Red\", \"NIR\"],";

    // Add values array
    json += "\"values\": [";
    for (int i = 0; i < 9; i++)
    {
        json += String(spectralData[i]);
        if (i < 8)
            json += ",";
    }
    json += "],";

    // Add other parameters
    json += "\"gain\": " + String(gainFactors[gainIndex]) + ",";
    float integrationTimeMs = (atimeSettings[atimeIndex] + 1) * 2.78;
    json += "\"integration_time\": " + String(integrationTimeMs) + ",";
    json += "\"selected_index\": " + String(wavelengthIndex);
    json += "}";

    server.send(200, "application/json", json);
}

void handleMeasure()
{
    // Take a new measurement
    takeMeasurement();

    // Return the same data format as handleData
    handleData();
}

void handleChangeWavelength()
{
    // Cycle to the next wavelength
    wavelengthIndex = (wavelengthIndex + 1) % 9;

    // Return the new selected index
    String json = "{\"selected_index\": " + String(wavelengthIndex) + "}";
    server.send(200, "application/json", json);
}

void handleAdjustGain()
{
    // Cycle to the next gain setting
    gainIndex = (gainIndex + 1) % 11;
    updateSettings();

    // Return the new gain value
    String json = "{\"gain\": " + String(gainFactors[gainIndex]) + "}";
    server.send(200, "application/json", json);
}

void handleAdjustIntegrationTime()
{
    // Cycle to the next integration time setting
    atimeIndex = (atimeIndex + 1) % 6;
    updateSettings();

    // Calculate integration time in milliseconds
    float integrationTimeMs = (atimeSettings[atimeIndex] + 1) * 2.78;

    // Return the new integration time
    String json = "{\"integration_time\": " + String(integrationTimeMs) + "}";
    server.send(200, "application/json", json);
}

void setup()
{
    // Initialize M5StickC Plus
    M5.begin();
    M5.Lcd.setRotation(3);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.println("Pocket Spectrometer");
    M5.Lcd.println("Web Server");

    // Initialize random seed for mock data variation
    randomSeed(analogRead(0));

    // Initialize I2C
    Wire.begin(32, 33); // SDA=32, SCL=33 for M5StickC Plus

    // Initialize AS7341 sensor (mock)
    sensor = AS7341(Wire);

    // Initialize spectral data array with zeros
    for (int i = 0; i < 9; i++)
    {
        spectralData[i] = 0;
    }

    // Apply initial settings
    updateSettings();

    // Connect to WiFi
    WiFi.begin(ssid, password);
    M5.Lcd.print("Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        M5.Lcd.print(".");
    }

    M5.Lcd.println("");
    M5.Lcd.println("WiFi connected!");
    M5.Lcd.print("IP: ");
    M5.Lcd.println(WiFi.localIP());

    // Set up web server routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/data", HTTP_GET, handleData);
    server.on("/measure", HTTP_GET, handleMeasure);
    server.on("/change_wavelength", HTTP_GET, handleChangeWavelength);
    server.on("/adjust_gain", HTTP_GET, handleAdjustGain);
    server.on("/adjust_integration", HTTP_GET, handleAdjustIntegrationTime);

    // Start server
    server.begin();
    M5.Lcd.println("HTTP server started");
    delay(2000);

    // Update display with IP address for easy connection
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 10);
    M5.Lcd.setTextSize(1);
    M5.Lcd.println("Spectrometer Web Server");
    M5.Lcd.println("Running at:");
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(0, 40);
    M5.Lcd.println(WiFi.localIP());
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 70);
    M5.Lcd.println("Connect to this IP");
    M5.Lcd.println("in your web browser");
    M5.Lcd.println("Press A to update data");

    // Take an initial measurement
    takeMeasurement();
}

void loop()
{
    server.handleClient();
    M5.update();

    // Check if button A is pressed to take a measurement
    if (M5.BtnA.wasPressed())
    {
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 10);
        M5.Lcd.println("Taking measurement...");

        // Take a new measurement
        takeMeasurement();

        M5.Lcd.println("Measurement complete!");
        M5.Lcd.println("Refresh web page to see data");
        M5.Lcd.setCursor(0, 40);
        M5.Lcd.setTextSize(2);
        M5.Lcd.println(WiFi.localIP());
        M5.Lcd.setTextSize(1);
    }

    // Check if button B is pressed to cycle through wavelengths
    if (M5.BtnB.wasPressed())
    {
        wavelengthIndex = (wavelengthIndex + 1) % 9;

        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(0, 10);
        M5.Lcd.println("Selected wavelength:");
        M5.Lcd.setTextSize(2);
        M5.Lcd.println(wavelengthNames[wavelengthIndex]);
        M5.Lcd.println(String(wavelengthValues[wavelengthIndex]) + " nm");
        M5.Lcd.setTextSize(1);
    }

    delay(10);
}
