// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/chromeos/cros/network_library.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

int32 GetPrefixLength(std::string netmask) {
  return NetworkIPConfig(std::string(), IPCONFIG_TYPE_UNKNOWN, std::string(),
      netmask, std::string(), std::string()).GetPrefixLength();
}

}  // namespace

TEST(NetworkLibraryTest, NetmaskToPrefixlen) {
  // Valid netmasks
  EXPECT_EQ(32, GetPrefixLength("255.255.255.255"));
  EXPECT_EQ(31, GetPrefixLength("255.255.255.254"));
  EXPECT_EQ(30, GetPrefixLength("255.255.255.252"));
  EXPECT_EQ(29, GetPrefixLength("255.255.255.248"));
  EXPECT_EQ(28, GetPrefixLength("255.255.255.240"));
  EXPECT_EQ(27, GetPrefixLength("255.255.255.224"));
  EXPECT_EQ(26, GetPrefixLength("255.255.255.192"));
  EXPECT_EQ(25, GetPrefixLength("255.255.255.128"));
  EXPECT_EQ(24, GetPrefixLength("255.255.255.0"));
  EXPECT_EQ(23, GetPrefixLength("255.255.254.0"));
  EXPECT_EQ(22, GetPrefixLength("255.255.252.0"));
  EXPECT_EQ(21, GetPrefixLength("255.255.248.0"));
  EXPECT_EQ(20, GetPrefixLength("255.255.240.0"));
  EXPECT_EQ(19, GetPrefixLength("255.255.224.0"));
  EXPECT_EQ(18, GetPrefixLength("255.255.192.0"));
  EXPECT_EQ(17, GetPrefixLength("255.255.128.0"));
  EXPECT_EQ(16, GetPrefixLength("255.255.0.0"));
  EXPECT_EQ(15, GetPrefixLength("255.254.0.0"));
  EXPECT_EQ(14, GetPrefixLength("255.252.0.0"));
  EXPECT_EQ(13, GetPrefixLength("255.248.0.0"));
  EXPECT_EQ(12, GetPrefixLength("255.240.0.0"));
  EXPECT_EQ(11, GetPrefixLength("255.224.0.0"));
  EXPECT_EQ(10, GetPrefixLength("255.192.0.0"));
  EXPECT_EQ(9, GetPrefixLength("255.128.0.0"));
  EXPECT_EQ(8, GetPrefixLength("255.0.0.0"));
  EXPECT_EQ(7, GetPrefixLength("254.0.0.0"));
  EXPECT_EQ(6, GetPrefixLength("252.0.0.0"));
  EXPECT_EQ(5, GetPrefixLength("248.0.0.0"));
  EXPECT_EQ(4, GetPrefixLength("240.0.0.0"));
  EXPECT_EQ(3, GetPrefixLength("224.0.0.0"));
  EXPECT_EQ(2, GetPrefixLength("192.0.0.0"));
  EXPECT_EQ(1, GetPrefixLength("128.0.0.0"));
  EXPECT_EQ(0, GetPrefixLength("0.0.0.0"));
  // Invalid netmasks
  EXPECT_EQ(-1, GetPrefixLength("255.255.255"));
  EXPECT_EQ(-1, GetPrefixLength("255.255.255.255.255"));
  EXPECT_EQ(-1, GetPrefixLength("255.255.255.255.0"));
  EXPECT_EQ(-1, GetPrefixLength("255.255.255.256"));
  EXPECT_EQ(-1, GetPrefixLength("255.255.255.1"));
  EXPECT_EQ(-1, GetPrefixLength("255.255.240.255"));
  EXPECT_EQ(-1, GetPrefixLength("255.0.0.255"));
  EXPECT_EQ(-1, GetPrefixLength("255.255.255.FF"));
  EXPECT_EQ(-1, GetPrefixLength("255,255,255,255"));
  EXPECT_EQ(-1, GetPrefixLength("255 255 255 255"));
}

TEST(NetworkLibraryTest, DecodeNonAsciiSSID) {

  // Sets network name.
  {
    std::string wifi_setname = "SSID TEST";
    std::string wifi_setname_result = "SSID TEST";
    WifiNetwork* wifi = new WifiNetwork("fw");
    wifi->SetName(wifi_setname);
    EXPECT_EQ(wifi->name(), wifi_setname_result);
    delete wifi;
  }

  // Truncates invalid UTF-8
  {
    std::string wifi_setname2 = "SSID TEST \x01\xff!";
    std::string wifi_setname2_result = "SSID TEST \xEF\xBF\xBD\xEF\xBF\xBD!";
    WifiNetwork* wifi = new WifiNetwork("fw");
    wifi->SetName(wifi_setname2);
    EXPECT_EQ(wifi->name(), wifi_setname2_result);
    delete wifi;
  }

  // UTF8 SSID
  {
    std::string wifi_utf8 = "UTF-8 \u3042\u3044\u3046";
    std::string wifi_utf8_result = "UTF-8 \xE3\x81\x82\xE3\x81\x84\xE3\x81\x86";
    WifiNetwork* wifi = new WifiNetwork("fw");
    wifi->SetSsid(wifi_utf8);
    EXPECT_EQ(wifi->name(), wifi_utf8_result);
    delete wifi;
  }

  // latin1 SSID -> UTF8 SSID
  {
    std::string wifi_latin1 = "latin-1 \xc0\xcb\xcc\xd6\xfb";
    std::string wifi_latin1_result = "latin-1 \u00c0\u00cb\u00cc\u00d6\u00fb";
    WifiNetwork* wifi = new WifiNetwork("fw");
    wifi->SetSsid(wifi_latin1);
    EXPECT_EQ(wifi->name(), wifi_latin1_result);
    delete wifi;
  }

  // Hex SSID
  {
    std::string wifi_hex = "5468697320697320484558205353494421";
    std::string wifi_hex_result = "This is HEX SSID!";
    WifiNetwork* wifi = new WifiNetwork("fw");
    wifi->SetHexSsid(wifi_hex);
    EXPECT_EQ(wifi->name(), wifi_hex_result);
    delete wifi;
  }
}

}  // namespace chromeos
