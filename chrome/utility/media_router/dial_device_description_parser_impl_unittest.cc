// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/utility/media_router/dial_device_description_parser_impl.h"

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kDeviceDescriptionWithService[] =
    "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n"
    "<specVersion>\n"
    "<major>1</major>\n"
    "<minor>0</minor>\n"
    "</specVersion>\n"
    "<URLBase>http://172.31.71.84:8008</URLBase>\n"
    "<device>\n"
    "<deviceType>urn:dial-multiscreen-org:device:dial:1</deviceType>\n"
    "<friendlyName>eureka9019</friendlyName>\n"
    "<manufacturer>Google Inc.</manufacturer>\n"
    "<modelName>Eureka Dongle</modelName>\n"
    "<serialNumber>123456789000</serialNumber>\n"
    "<UDN>uuid:d90dda41-8fa0-61ac-0567-f949d3e34b0e</UDN>\n"
    "<serviceList>\n"
    "<service>\n"
    "<serviceType>urn:dial-multiscreen-org:service:dial:1</serviceType>\n"
    "<serviceId>urn:dial-multiscreen-org:serviceId:dial</serviceId>\n"
    "<controlURL>/ssdp/notfound</controlURL>\n"
    "<eventSubURL>/ssdp/notfound</eventSubURL>\n"
    "<SCPDURL>/ssdp/notfound</SCPDURL>\n"
    "<servicedata xmlns=\"uri://cloudview.google.com/...\">\n"
    "</servicedata>\n"
    "</service>\n"
    "</serviceList>\n"
    "</device>\n"
    "</root>\n";

constexpr char kDeviceDescriptionWithoutService[] =
    "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\n"
    "<specVersion>\n"
    "<major>1</major>\n"
    "<minor>0</minor>\n"
    "</specVersion>\n"
    "<URLBase>http://172.31.71.84:8008</URLBase>\n"
    "<device>\n"
    "<deviceType>urn:dial-multiscreen-org:device:dial:1</deviceType>\n"
    "<friendlyName>eureka9020</friendlyName>\n"
    "<manufacturer>Google Inc.</manufacturer>\n"
    "<modelName>Eureka Dongle</modelName>\n"
    "<serialNumber>123456789000</serialNumber>\n"
    "<UDN>uuid:d90dda41-8fa0-61ac-0567-f949d3e34b0f</UDN>\n"
    "</device>\n"
    "</root>\n";

std::string& Replace(std::string& input,
                     const std::string& from,
                     const std::string& to) {
  size_t pos = input.find(from);
  if (pos == std::string::npos)
    return input;

  return input.replace(pos, from.size(), to);
}

}  // namespace

namespace media_router {

class DialDeviceDescriptionParserImplTest : public testing::Test {
 public:
  DialDeviceDescriptionParserImplTest() {}
  chrome::mojom::DialDeviceDescriptionPtr Parse(
      const std::string& xml,
      chrome::mojom::DialParsingError expected_error) {
    chrome::mojom::DialParsingError error;
    auto out = parser_.Parse(xml, &error);
    EXPECT_EQ(expected_error, error);
    return out;
  }

 private:
  DialDeviceDescriptionParserImpl parser_;
  DISALLOW_COPY_AND_ASSIGN(DialDeviceDescriptionParserImplTest);
};

TEST_F(DialDeviceDescriptionParserImplTest, TestInvalidXml) {
  chrome::mojom::DialDeviceDescriptionPtr device_description =
      Parse("", chrome::mojom::DialParsingError::NONE);
  ASSERT_TRUE(device_description);
  EXPECT_TRUE(device_description->unique_id.empty());
}

TEST_F(DialDeviceDescriptionParserImplTest, TestParse) {
  std::string xml_text(kDeviceDescriptionWithService);

  chrome::mojom::DialDeviceDescriptionPtr device_description =
      Parse(xml_text, chrome::mojom::DialParsingError::NONE);
  ASSERT_TRUE(device_description);

  EXPECT_EQ("urn:dial-multiscreen-org:device:dial:1",
            device_description->device_type);
  EXPECT_EQ("eureka9019", device_description->friendly_name);
  EXPECT_EQ("Eureka Dongle", device_description->model_name);
  EXPECT_EQ("uuid:d90dda41-8fa0-61ac-0567-f949d3e34b0e",
            device_description->unique_id);
}

TEST_F(DialDeviceDescriptionParserImplTest, TestParseWithSpecialCharacter) {
  std::string old_name = "<friendlyName>eureka9019</friendlyName>";
  std::string new_name = "<friendlyName>Samsung LED40\'s</friendlyName>";

  std::string xml_text(kDeviceDescriptionWithService);
  xml_text = Replace(xml_text, old_name, new_name);

  chrome::mojom::DialDeviceDescriptionPtr device_description =
      Parse(xml_text, chrome::mojom::DialParsingError::NONE);
  ASSERT_TRUE(device_description);

  EXPECT_EQ("urn:dial-multiscreen-org:device:dial:1",
            device_description->device_type);
  EXPECT_EQ("Samsung LED40\'s", device_description->friendly_name);
  EXPECT_EQ("Eureka Dongle", device_description->model_name);
  EXPECT_EQ("uuid:d90dda41-8fa0-61ac-0567-f949d3e34b0e",
            device_description->unique_id);
}

TEST_F(DialDeviceDescriptionParserImplTest,
       TestParseWithoutFriendlyNameModelName) {
  std::string friendly_name = "<friendlyName>eureka9020</friendlyName>";
  std::string model_name = "<modelName>Eureka Dongle</modelName>";

  std::string xml_text(kDeviceDescriptionWithoutService);
  xml_text = Replace(xml_text, friendly_name, "");
  xml_text = Replace(xml_text, model_name, "");

  chrome::mojom::DialDeviceDescriptionPtr device_description =
      Parse(xml_text, chrome::mojom::DialParsingError::NONE);
  ASSERT_TRUE(device_description);
  EXPECT_TRUE(device_description->friendly_name.empty());
  EXPECT_TRUE(device_description->model_name.empty());
}

TEST_F(DialDeviceDescriptionParserImplTest, TestParseWithoutFriendlyName) {
  std::string friendly_name = "<friendlyName>eureka9020</friendlyName>";

  std::string xml_text(kDeviceDescriptionWithoutService);
  xml_text = Replace(xml_text, friendly_name, "");

  chrome::mojom::DialDeviceDescriptionPtr device_description =
      Parse(xml_text, chrome::mojom::DialParsingError::NONE);
  ASSERT_TRUE(device_description);

  EXPECT_EQ("urn:dial-multiscreen-org:device:dial:1",
            device_description->device_type);
  EXPECT_EQ("Eureka Dongle [4b0f]", device_description->friendly_name);
  EXPECT_EQ("Eureka Dongle", device_description->model_name);
  EXPECT_EQ("uuid:d90dda41-8fa0-61ac-0567-f949d3e34b0f",
            device_description->unique_id);
}

}  // namespace media_router
