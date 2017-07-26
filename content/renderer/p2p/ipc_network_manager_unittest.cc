// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/ipc_network_manager.h"

#include <algorithm>
#include <memory>

#include "content/renderer/p2p/network_list_manager.h"
#include "net/base/ip_address.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockP2PSocketDispatcher : public NetworkListManager {
 public:
  void AddNetworkListObserver(
      NetworkListObserver* network_list_observer) override {}

  void RemoveNetworkListObserver(
      NetworkListObserver* network_list_observer) override {}

  ~MockP2PSocketDispatcher() override {}
};

}  // namespace

// 2 IPv6 addresses with only last digit different.
static const char kIPv6PublicAddrString1[] =
    "2401:fa00:4:1000:be30:5b30:50e5:c3";
static const char kIPv6PublicAddrString2[] =
    "2401:fa00:4:1000:be30:5b30:50e5:c4";
static const char kIPv4MappedAddrString[] = "::ffff:38.32.0.0";

class IpcNetworkManagerTest : public testing::Test {
 public:
  IpcNetworkManagerTest()
      : network_list_manager_(new MockP2PSocketDispatcher()),
        network_manager_(new IpcNetworkManager(network_list_manager_.get())) {}

 protected:
  std::unique_ptr<MockP2PSocketDispatcher> network_list_manager_;
  std::unique_ptr<IpcNetworkManager> network_manager_;
};

// Test overall logic of IpcNetworkManager on OnNetworkListChanged
// that it should group addresses with the same network key under
// single Network class. This also tests the logic inside
// IpcNetworkManager in addition to MergeNetworkList.
// TODO(guoweis): disable this test case for now until fix for webrtc
// issue 19249005 integrated into chromium
TEST_F(IpcNetworkManagerTest, TestMergeNetworkList) {
  net::NetworkInterfaceList list;
  net::IPAddress ip;
  std::vector<rtc::Network*> networks;
  rtc::IPAddress ip_address;

  // Add 2 networks with the same prefix and prefix length.
  EXPECT_TRUE(ip.AssignFromIPLiteral(kIPv6PublicAddrString1));
  list.push_back(net::NetworkInterface(
      "em1", "em1", 0, net::NetworkChangeNotifier::CONNECTION_UNKNOWN, ip, 64,
      net::IP_ADDRESS_ATTRIBUTE_NONE));

  EXPECT_TRUE(ip.AssignFromIPLiteral(kIPv6PublicAddrString2));
  list.push_back(net::NetworkInterface(
      "em1", "em1", 0, net::NetworkChangeNotifier::CONNECTION_UNKNOWN, ip, 64,
      net::IP_ADDRESS_ATTRIBUTE_NONE));

  network_manager_->OnNetworkListChanged(list, net::IPAddress(),
                                         net::IPAddress());
  network_manager_->GetNetworks(&networks);
  EXPECT_EQ(1uL, networks.size());
  EXPECT_EQ(2uL, networks[0]->GetIPs().size());

  // Add another network with different prefix length, should result in
  // a different network.
  networks.clear();
  list.push_back(net::NetworkInterface(
      "em1", "em1", 0, net::NetworkChangeNotifier::CONNECTION_UNKNOWN, ip, 48,
      net::IP_ADDRESS_ATTRIBUTE_NONE));

  // Push an unknown address as the default address.
  EXPECT_TRUE(ip.AssignFromIPLiteral(kIPv4MappedAddrString));
  network_manager_->OnNetworkListChanged(list, net::IPAddress(), ip);

  // The unknown default address should be ignored.
  EXPECT_FALSE(network_manager_->GetDefaultLocalAddress(AF_INET6, &ip_address));

  network_manager_->GetNetworks(&networks);

  // Verify we have 2 networks now.
  EXPECT_EQ(2uL, networks.size());
  // Verify the network with prefix length of 64 has 2 IP addresses.
  auto network_with_two_ips = std::find_if(
      networks.begin(), networks.end(),
      [](rtc::Network* network) { return network->prefix_length() == 64; });
  ASSERT_NE(networks.end(), network_with_two_ips);
  EXPECT_EQ(2uL, (*network_with_two_ips)->GetIPs().size());
  // IPs should be in the same order as the list passed into
  // OnNetworkListChanged.
  EXPECT_TRUE(rtc::IPFromString(kIPv6PublicAddrString1, &ip_address));
  EXPECT_EQ((*network_with_two_ips)->GetIPs()[0], ip_address);
  EXPECT_TRUE(rtc::IPFromString(kIPv6PublicAddrString2, &ip_address));
  EXPECT_EQ((*network_with_two_ips)->GetIPs()[1], ip_address);
  // Verify the network with prefix length of 48 has 1 IP address.
  auto network_with_one_ip = std::find_if(
      networks.begin(), networks.end(),
      [](rtc::Network* network) { return network->prefix_length() == 48; });
  ASSERT_NE(networks.end(), network_with_one_ip);
  EXPECT_EQ(1uL, (*network_with_one_ip)->GetIPs().size());
  EXPECT_TRUE(rtc::IPFromString(kIPv6PublicAddrString2, &ip_address));
  EXPECT_EQ((*network_with_one_ip)->GetIPs()[0], ip_address);
}

}  // namespace content
