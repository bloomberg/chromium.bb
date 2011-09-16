// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/chromeos/cros/cros_library.h"
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

// Create a stub libcros for testing NetworkLibrary functionality through
// NetworkLibraryStubImpl.
// NOTE: It would be of little value to test stub functions that simply return
// predefined values, e.g. ethernet_available(). However, many other functions
// such as connected_network() return values which are set indirectly and thus
// we can test the logic of those setters.

class NetworkLibraryStubTest : public testing::Test {
 public:
  NetworkLibraryStubTest() : cros_(NULL) {}

 protected:
  virtual void SetUp() {
    cros_ = CrosLibrary::Get()->GetNetworkLibrary();
    ASSERT_TRUE(cros_) << "GetNetworkLibrary() Failed!";
  }
  virtual void TearDown() {
    cros_ = NULL;
  }

  ScopedStubCrosEnabler cros_stub_;
  NetworkLibrary* cros_;
};

// Default stub state:
// eth1: connected
// wifi1: connected
// wifi2: connecting
// wifi3: disconnected, WEP
// wifi4: disconnected, 8021x
// wifi5: disconnected
// wifi6: disconnected
// cellular1: connecting, activated, roaming
// cellular2: disconnected, activated, not roaming
// vpn1: disconnected, L2TP/IPsec + PSK
// vpn2: connected, L2TP/IPsec + user cert
// vpn3: disconnected, OpenVpn

TEST_F(NetworkLibraryStubTest, NetworkLibraryAccessors) {
  // Ethernet
  ASSERT_NE(static_cast<const EthernetNetwork*>(NULL),
            cros_->ethernet_network());
  EXPECT_EQ("eth1", cros_->ethernet_network()->service_path());
  EXPECT_NE(static_cast<const Network*>(NULL),
            cros_->FindNetworkByPath("eth1"));
  EXPECT_TRUE(cros_->ethernet_connected());
  EXPECT_FALSE(cros_->ethernet_connecting());

  // Wifi
  ASSERT_NE(static_cast<const WifiNetwork*>(NULL), cros_->wifi_network());
  EXPECT_EQ("wifi1", cros_->wifi_network()->service_path());
  EXPECT_NE(static_cast<const WifiNetwork*>(NULL),
            cros_->FindWifiNetworkByPath("wifi1"));
  EXPECT_TRUE(cros_->wifi_connected());
  EXPECT_FALSE(cros_->wifi_connecting());  // Only true for active wifi.
  EXPECT_EQ(6U, cros_->wifi_networks().size());

  // Cellular
  ASSERT_NE(static_cast<const CellularNetwork*>(NULL),
            cros_->cellular_network());
  EXPECT_EQ("cellular1", cros_->cellular_network()->service_path());
  EXPECT_NE(static_cast<const CellularNetwork*>(NULL),
            cros_->FindCellularNetworkByPath("cellular1"));
  EXPECT_FALSE(cros_->cellular_connected());
  EXPECT_TRUE(cros_->cellular_connecting());
  EXPECT_EQ(2U, cros_->cellular_networks().size());

  // VPN
  ASSERT_NE(static_cast<const VirtualNetwork*>(NULL), cros_->virtual_network());
  EXPECT_EQ("vpn2", cros_->virtual_network()->service_path());
  EXPECT_NE(static_cast<const VirtualNetwork*>(NULL),
            cros_->FindVirtualNetworkByPath("vpn1"));
  EXPECT_TRUE(cros_->virtual_network_connected());
  EXPECT_FALSE(cros_->virtual_network_connecting());
  EXPECT_EQ(3U, cros_->virtual_networks().size());

  // Active network and global state
  EXPECT_TRUE(cros_->Connected());
  EXPECT_TRUE(cros_->Connecting());
  EXPECT_EQ("eth1", cros_->active_network()->service_path());
  EXPECT_EQ("eth1", cros_->connected_network()->service_path());
  // The "wifi1" is connected, so we do not return "wifi2" for the connecting
  // network. There is no conencted cellular network, so "cellular1" is
  // returned by connecting_network().
  EXPECT_EQ("cellular1", cros_->connecting_network()->service_path());
}

TEST_F(NetworkLibraryStubTest, NetworkConnect) {
  WifiNetwork* wifi1 = cros_->FindWifiNetworkByPath("wifi1");
  ASSERT_NE(static_cast<const WifiNetwork*>(NULL), wifi1);
  EXPECT_TRUE(wifi1->connected());
  cros_->DisconnectFromNetwork(wifi1);
  EXPECT_FALSE(wifi1->connected());
  EXPECT_TRUE(cros_->CanConnectToNetwork(wifi1));
  cros_->ConnectToWifiNetwork(wifi1);
  EXPECT_TRUE(wifi1->connected());
}

// TODO(stevenjb): Test remembered networks.

// TODO(stevenjb): Test network profiles.

// TODO(stevenjb): Test network devices.

// TODO(stevenjb): Test data plans.

// TODO(stevenjb): Test monitor network / device.

}  // namespace chromeos
