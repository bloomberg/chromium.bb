// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/proximity_auth_system.h"
#include "components/proximity_auth/remote_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {

TEST(ProximityAuthSystemTest, GetRemoteDevices) {
  RemoteDevice device1 = {"device 1"};
  RemoteDevice device2 = {"device 2"};

  std::vector<RemoteDevice> device_list;
  device_list.push_back(device1);
  device_list.push_back(device2);

  ProximityAuthSystem system(device_list);

  const std::vector<RemoteDevice>& returned_list = system.GetRemoteDevices();
  ASSERT_EQ(2u, returned_list.size());
  EXPECT_EQ(device1.name, returned_list[0].name);
  EXPECT_EQ(device2.name, returned_list[1].name);
}

}  // namespace proximity_auth
