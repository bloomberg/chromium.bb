// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "network_change_notifier.h"

#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(NetworkChangeNotifierTest, InterfacesToConnectionType) {
  NetworkInterfaceList list;

  // Test empty list.
  EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list),
            NetworkChangeNotifier::CONNECTION_UNKNOWN);

  NetworkInterface interface;
  for (int i = NetworkChangeNotifier::CONNECTION_UNKNOWN;
       i < NetworkChangeNotifier::CONNECTION_LAST;
       i++) {
    // Check individual types.
    list.clear();
    interface.type = static_cast<NetworkChangeNotifier::ConnectionType>(i);
    list.push_back(interface);
    EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list), i);
    // Check two types.
    for (int j = NetworkChangeNotifier::CONNECTION_UNKNOWN;
         j < NetworkChangeNotifier::CONNECTION_LAST;
         j++) {
      list.clear();
      interface.type = static_cast<NetworkChangeNotifier::ConnectionType>(i);
      list.push_back(interface);
      interface.type = static_cast<NetworkChangeNotifier::ConnectionType>(j);
      list.push_back(interface);
      EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list),
                i == j ? i : NetworkChangeNotifier::CONNECTION_UNKNOWN);
    }
  }

#if defined(OS_WIN)
  // Ignore fake Teredo interface.
  list.clear();
  interface.type = NetworkChangeNotifier::CONNECTION_4G;
  interface.friendly_name = "Teredo Tunneling Pseudo-Interface";
  list.push_back(interface);
  // Verify Teredo interface type ignored.
  EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list),
            NetworkChangeNotifier::CONNECTION_UNKNOWN);
  // Verify type of non-Teredo interface used.
  interface.type = NetworkChangeNotifier::CONNECTION_3G;
  interface.friendly_name = "";
  list.push_back(interface);
  EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list),
            NetworkChangeNotifier::CONNECTION_3G);
  // Reverse elements.
  list.push_back(list[0]);
  list.erase(list.begin());
  EXPECT_EQ(NetworkChangeNotifier::ConnectionTypeFromInterfaceList(list),
            NetworkChangeNotifier::CONNECTION_3G);
#endif
}

}  // namespace net
