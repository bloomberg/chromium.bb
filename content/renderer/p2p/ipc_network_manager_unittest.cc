// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "content/renderer/p2p/ipc_network_manager.h"
#include "content/renderer/p2p/network_list_manager.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockP2PSocketDispatcher : public NetworkListManager {
 public:
  virtual void AddNetworkListObserver(
      NetworkListObserver* network_list_observer) OVERRIDE {}

  virtual void RemoveNetworkListObserver(
      NetworkListObserver* network_list_observer) OVERRIDE {}

  virtual ~MockP2PSocketDispatcher() {}
};

}  // namespace

// 2 IPv6 addresses with only last digit different.
static const char kIPv6PublicAddrString1[] =
    "2401:fa00:4:1000:be30:5bff:fee5:c3";
static const char kIPv6PublicAddrString2[] =
    "2401:fa00:4:1000:be30:5bff:fee5:c4";

class IpcNetworkManagerTest : public testing::Test {
 public:
  IpcNetworkManagerTest()
      : network_list_manager_(new MockP2PSocketDispatcher()),
        network_manager_(new IpcNetworkManager(network_list_manager_.get())) {}

 protected:
  scoped_ptr<MockP2PSocketDispatcher> network_list_manager_;
  scoped_ptr<IpcNetworkManager> network_manager_;
};

// Test overall logic of IpcNetworkManager on OnNetworkListChanged
// that it should group addresses with the same network key under
// single Network class. This also tests the logic inside
// IpcNetworkManager in addition to MergeNetworkList.
// TODO(guoweis): disable this test case for now until fix for webrtc
// issue 19249005 integrated into chromium
TEST_F(IpcNetworkManagerTest, DISABLED_TestMergeNetworkList) {
  net::NetworkInterfaceList list;
  net::IPAddressNumber ip_number;
  std::vector<rtc::Network*> networks;
  rtc::IPAddress ip_address;

  // Add 2 networks with the same prefix and prefix length.
  EXPECT_TRUE(net::ParseIPLiteralToNumber(kIPv6PublicAddrString1, &ip_number));
  list.push_back(
      net::NetworkInterface("em1",
                            "em1",
                            0,
                            net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
                            ip_number,
                            64,
                            net::IP_ADDRESS_ATTRIBUTE_NONE));

  EXPECT_TRUE(net::ParseIPLiteralToNumber(kIPv6PublicAddrString2, &ip_number));
  list.push_back(
      net::NetworkInterface("em1",
                            "em1",
                            0,
                            net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
                            ip_number,
                            64,
                            net::IP_ADDRESS_ATTRIBUTE_NONE));

  network_manager_->OnNetworkListChanged(list);
  network_manager_->GetNetworks(&networks);
  EXPECT_EQ(1uL, networks.size());
  EXPECT_EQ(2uL, networks[0]->GetIPs().size());

  // Add another network with different prefix length, should result in
  // a different network.
  networks.clear();
  list.push_back(
      net::NetworkInterface("em1",
                            "em1",
                            0,
                            net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
                            ip_number,
                            48,
                            net::IP_ADDRESS_ATTRIBUTE_NONE));

  network_manager_->OnNetworkListChanged(list);

  network_manager_->GetNetworks(&networks);

  // Verify we have 2 networks now.
  EXPECT_EQ(2uL, networks.size());
  // Verify the network with prefix length of 64 has 2 IP addresses.
  EXPECT_EQ(64, networks[1]->prefix_length());
  EXPECT_EQ(2uL, networks[1]->GetIPs().size());
  EXPECT_TRUE(rtc::IPFromString(kIPv6PublicAddrString1, &ip_address));
  EXPECT_EQ(networks[1]->GetIPs()[0], ip_address);
  EXPECT_TRUE(rtc::IPFromString(kIPv6PublicAddrString2, &ip_address));
  EXPECT_EQ(networks[1]->GetIPs()[1], ip_address);
  // Verify the network with prefix length of 48 has 2 IP addresses.
  EXPECT_EQ(48, networks[0]->prefix_length());
  EXPECT_EQ(1uL, networks[0]->GetIPs().size());
  EXPECT_TRUE(rtc::IPFromString(kIPv6PublicAddrString2, &ip_address));
  EXPECT_EQ(networks[0]->GetIPs()[0], ip_address);
}

}  // namespace content
