// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/discovery/netbios_host_locator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace smb_client {
namespace {

// Helper method to create a NetworkInterface for testing.
net::NetworkInterface CreateNetworkInterface(
    const net::IPAddress& address,
    uint32_t prefix_length,
    net::NetworkChangeNotifier::ConnectionType type) {
  net::NetworkInterface interface;
  interface.address = address;
  interface.prefix_length = prefix_length;
  interface.type = type;
  return interface;
}

}  // namespace

class NetBiosHostLocatorTest : public testing::Test {
 public:
  NetBiosHostLocatorTest() = default;
  ~NetBiosHostLocatorTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetBiosHostLocatorTest);
};

// Calculate broadcast address correctly calculates the broadcast address
// of a NetworkInterface.
TEST_F(NetBiosHostLocatorTest, CalculateBroadcastAddress) {
  const net::NetworkInterface interface = CreateNetworkInterface(
      net::IPAddress(192, 168, 50, 152), 24 /* prefix_length */,
      net::NetworkChangeNotifier::CONNECTION_WIFI);

  EXPECT_EQ(net::IPAddress(192, 168, 50, 255),
            CalculateBroadcastAddress(interface));
}

// ShouldUseInterface returns true for Wifi and Ethernet interfaces but false
// for other types of interfaces.
TEST_F(NetBiosHostLocatorTest, ShouldUseWifiAndEthernetInterfaces) {
  const net::NetworkInterface interface_wifi = CreateNetworkInterface(
      net::IPAddress::IPv4Localhost(), 24 /* prefix_length */,
      net::NetworkChangeNotifier::CONNECTION_WIFI);

  const net::NetworkInterface interface_ethernet = CreateNetworkInterface(
      net::IPAddress::IPv4Localhost(), 24 /* prefix_length */,
      net::NetworkChangeNotifier::CONNECTION_WIFI);

  const net::NetworkInterface interface_bluetooth = CreateNetworkInterface(
      net::IPAddress::IPv4Localhost(), 24 /* prefix_length */,
      net::NetworkChangeNotifier::CONNECTION_BLUETOOTH);

  EXPECT_TRUE(ShouldUseInterface(interface_wifi));
  EXPECT_TRUE(ShouldUseInterface(interface_ethernet));
  EXPECT_FALSE(ShouldUseInterface(interface_bluetooth));
}

// ShouldUseInterface returns true for IPv4 interfaces but false for IPv6
// interfaces
TEST_F(NetBiosHostLocatorTest, OnlyProcessIPv4Interfaces) {
  const net::NetworkInterface interface_ipv4 = CreateNetworkInterface(
      net::IPAddress::IPv4Localhost(), 24 /* prefix_length */,
      net::NetworkChangeNotifier::CONNECTION_WIFI);

  const net::NetworkInterface interface_ipv6 = CreateNetworkInterface(
      net::IPAddress::IPv6Localhost(), 24 /* prefix_length */,
      net::NetworkChangeNotifier::CONNECTION_WIFI);

  EXPECT_TRUE(ShouldUseInterface(interface_ipv4));
  EXPECT_FALSE(ShouldUseInterface(interface_ipv6));
}

}  // namespace smb_client
}  // namespace chromeos
