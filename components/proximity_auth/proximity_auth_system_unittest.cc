// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/proximity_auth_system.h"
#include "components/proximity_auth/remote_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {

TEST(ProximityAuthSystemTest, GetRemoteDevices) {
  RemoteDevice device1("example@gmail.com", "device1", "public_key1",
                       RemoteDevice::BLUETOOTH_CLASSIC, "bt_addr1", "psk1", "");
  RemoteDevice device2("example@gmail.com", "device2", "public_key2",
                       RemoteDevice::BLUETOOTH_LE, "bt_addr2", "psk2", "");

  std::vector<RemoteDevice> device_list;
  device_list.push_back(device1);
  device_list.push_back(device2);

  ProximityAuthSystem system(device_list);

  const std::vector<RemoteDevice>& returned_list = system.GetRemoteDevices();
  ASSERT_EQ(2u, returned_list.size());
  EXPECT_EQ(device1.name, returned_list[0].name);
  EXPECT_EQ(device1.public_key, returned_list[0].public_key);
  EXPECT_EQ(device2.name, returned_list[1].name);
  EXPECT_EQ(device2.public_key, returned_list[1].public_key);
}

}  // namespace proximity_auth
