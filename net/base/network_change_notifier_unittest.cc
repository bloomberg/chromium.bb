// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier.h"

#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Note: This test is subject to the host's OS and network connection. This test
// is not future-proof. New standards will come about necessitating the need to
// alter the ranges of these tests.
TEST(NetworkChangeNotifierTest, NetMaxBandwidthRange) {
  NetworkChangeNotifier::ConnectionType connection_type =
      NetworkChangeNotifier::GetConnectionType();
  double max_bandwidth = NetworkChangeNotifier::GetMaxBandwidth();

  // Always accept infinity as it's the default value if the bandwidth is
  // unknown.
  if (max_bandwidth == std::numeric_limits<double>::infinity()) {
    EXPECT_NE(NetworkChangeNotifier::CONNECTION_NONE, connection_type);
    return;
  }

  switch (connection_type) {
    case NetworkChangeNotifier::CONNECTION_UNKNOWN:
      EXPECT_EQ(std::numeric_limits<double>::infinity(), max_bandwidth);
      break;
    case NetworkChangeNotifier::CONNECTION_ETHERNET:
      EXPECT_GE(10.0, max_bandwidth);
      EXPECT_LE(10000.0, max_bandwidth);
      break;
    case NetworkChangeNotifier::CONNECTION_WIFI:
      EXPECT_GE(1.0, max_bandwidth);
      EXPECT_LE(7000.0, max_bandwidth);
      break;
    case NetworkChangeNotifier::CONNECTION_2G:
      EXPECT_GE(0.01, max_bandwidth);
      EXPECT_LE(0.384, max_bandwidth);
      break;
    case NetworkChangeNotifier::CONNECTION_3G:
      EXPECT_GE(2.0, max_bandwidth);
      EXPECT_LE(42.0, max_bandwidth);
      break;
    case NetworkChangeNotifier::CONNECTION_4G:
      EXPECT_GE(100.0, max_bandwidth);
      EXPECT_LE(100.0, max_bandwidth);
      break;
    case NetworkChangeNotifier::CONNECTION_NONE:
      EXPECT_EQ(0.0, max_bandwidth);
      break;
    case NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      EXPECT_GE(1.0, max_bandwidth);
      EXPECT_LE(24.0, max_bandwidth);
      break;
  }
}

TEST(NetworkChangeNotifierTest, ConnectionTypeFromInterfaceList) {
  NetworkInterfaceList list;

  // Test empty list.
  EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list),
            NetworkChangeNotifier::CONNECTION_NONE);

  for (int i = NetworkChangeNotifier::CONNECTION_UNKNOWN;
       i <= NetworkChangeNotifier::CONNECTION_LAST; i++) {
    // Check individual types.
    NetworkInterface interface;
    interface.type = static_cast<NetworkChangeNotifier::ConnectionType>(i);
    list.clear();
    list.push_back(interface);
    EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list), i);
    // Check two types.
    for (int j = NetworkChangeNotifier::CONNECTION_UNKNOWN;
         j <= NetworkChangeNotifier::CONNECTION_LAST; j++) {
      list.clear();
      interface.type = static_cast<NetworkChangeNotifier::ConnectionType>(i);
      list.push_back(interface);
      interface.type = static_cast<NetworkChangeNotifier::ConnectionType>(j);
      list.push_back(interface);
      EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list),
                i == j ? i : NetworkChangeNotifier::CONNECTION_UNKNOWN);
    }
  }
}

}  // namespace net
