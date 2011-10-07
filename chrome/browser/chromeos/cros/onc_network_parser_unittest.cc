// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/onc_network_parser.h"

#include "base/values.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(OncNetworkParserTest, TestCreateNetworkWifi1) {
  std::string test_blob(
      "{"
      "  \"NetworkConfigurations\": [{"
      "    \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5045}\","
      "    \"Type\": \"WiFi\","
      "    \"WiFi\": {"
      "      \"Security\": \"WEP\","
      "      \"SSID\": \"ssid\","
      "      \"Passphrase\": \"pass\","
      "    }"
      "  }]"
      "}");
  OncNetworkParser parser(test_blob);

  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  EXPECT_EQ(0, parser.GetCertificatesSize());
  EXPECT_FALSE(parser.ParseNetwork(1));
  Network* network = parser.ParseNetwork(0);
  ASSERT_TRUE(network);

  EXPECT_EQ(network->type(), chromeos::TYPE_WIFI);
  WifiNetwork* wifi = static_cast<WifiNetwork*>(network);
  EXPECT_EQ(wifi->encryption(), chromeos::SECURITY_WEP);
  EXPECT_EQ(wifi->name(), "ssid");
  EXPECT_EQ(wifi->auto_connect(), false);
  EXPECT_EQ(wifi->passphrase(), "pass");
}

TEST(OncNetworkParserTest, TestCreateNetworkWifiEAP1) {
  std::string test_blob(
      "{"
      "  \"NetworkConfigurations\": [{"
      "    \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5045}\","
      "    \"Type\": \"WiFi\","
      "    \"WiFi\": {"
      "      \"Security\": \"WPA2\","
      "      \"SSID\": \"ssid\","
      "      \"AutoConnect\": true,"
      "      \"EAP\": {"
      "        \"Outer\": \"PEAP\","
      "        \"UseSystemCAs\": false,"
      "      }"
      "    }"
      "  }]"
      "}");
  OncNetworkParser parser(test_blob);

  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  EXPECT_EQ(0, parser.GetCertificatesSize());
  EXPECT_FALSE(parser.ParseNetwork(1));
  Network* network = parser.ParseNetwork(0);
  ASSERT_TRUE(network);

  EXPECT_EQ(network->type(), chromeos::TYPE_WIFI);
  WifiNetwork* wifi = static_cast<WifiNetwork*>(network);
  EXPECT_EQ(wifi->encryption(), chromeos::SECURITY_8021X);
  EXPECT_EQ(wifi->name(), "ssid");
  EXPECT_EQ(wifi->auto_connect(), true);
  EXPECT_EQ(wifi->eap_method(), EAP_METHOD_PEAP);
  EXPECT_EQ(wifi->eap_use_system_cas(), false);
}

TEST(OncNetworkParserTest, TestCreateNetworkWifiEAP2) {
  std::string test_blob(
      "{"
      "  \"NetworkConfigurations\": [{"
      "    \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5045}\","
      "    \"Type\": \"WiFi\","
      "    \"WiFi\": {"
      "      \"Security\": \"WPA2\","
      "      \"SSID\": \"ssid\","
      "      \"AutoConnect\": false,"
      "      \"EAP\": {"
      "        \"Outer\": \"LEAP\","
      "        \"Identity\": \"user\","
      "        \"Password\": \"pass\","
      "        \"AnonymousIdentity\": \"anon\","
      "      }"
      "    }"
      "  }]"
      "}");
  OncNetworkParser parser(test_blob);

  EXPECT_EQ(1, parser.GetNetworkConfigsSize());
  EXPECT_EQ(0, parser.GetCertificatesSize());
  EXPECT_FALSE(parser.ParseNetwork(1));
  Network* network = parser.ParseNetwork(0);
  ASSERT_TRUE(network);

  EXPECT_EQ(network->type(), chromeos::TYPE_WIFI);
  WifiNetwork* wifi = static_cast<WifiNetwork*>(network);
  EXPECT_EQ(wifi->encryption(), chromeos::SECURITY_8021X);
  EXPECT_EQ(wifi->name(), "ssid");
  EXPECT_EQ(wifi->auto_connect(), false);
  EXPECT_EQ(wifi->eap_method(), EAP_METHOD_LEAP);
  EXPECT_EQ(wifi->eap_use_system_cas(), true);
  EXPECT_EQ(wifi->eap_identity(), "user");
  EXPECT_EQ(wifi->eap_passphrase(), "pass");
  EXPECT_EQ(wifi->eap_anonymous_identity(), "anon");
}

}  // namespace chromeos
