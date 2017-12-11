// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_discovery.h"

#include <utility>

#include "device/u2f/mock_u2f_discovery.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::testing::_;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

TEST(U2fDiscoveryTest, TestAddAndRemoveObserver) {
  MockU2fDiscovery discovery;
  MockU2fDiscoveryObserver observer;
  EXPECT_FALSE(discovery.GetObservers().HasObserver(&observer));

  discovery.AddObserver(&observer);
  EXPECT_TRUE(discovery.GetObservers().HasObserver(&observer));

  discovery.RemoveObserver(&observer);
  EXPECT_FALSE(discovery.GetObservers().HasObserver(&observer));
}

TEST(U2fDiscoveryTest, TestNotifications) {
  MockU2fDiscovery discovery;
  MockU2fDiscoveryObserver observer;
  discovery.AddObserver(&observer);

  EXPECT_CALL(observer, DiscoveryStarted(&discovery, true));
  discovery.NotifyDiscoveryStarted(true);

  EXPECT_CALL(observer, DiscoveryStarted(&discovery, false));
  discovery.NotifyDiscoveryStarted(false);

  EXPECT_CALL(observer, DiscoveryStopped(&discovery, true));
  discovery.NotifyDiscoveryStopped(true);

  EXPECT_CALL(observer, DiscoveryStopped(&discovery, false));
  discovery.NotifyDiscoveryStopped(false);

  MockU2fDevice device;
  EXPECT_CALL(observer, DeviceAdded(&discovery, &device));
  discovery.NotifyDeviceAdded(&device);

  EXPECT_CALL(observer, DeviceRemoved(&discovery, &device));
  discovery.NotifyDeviceRemoved(&device);
}

TEST(U2fDiscoveryTest, TestAddRemoveDevices) {
  MockU2fDiscovery discovery;
  MockU2fDiscoveryObserver observer;
  discovery.AddObserver(&observer);

  // Expect successful insertion.
  auto device0 = std::make_unique<MockU2fDevice>();
  auto* device0_raw = device0.get();
  EXPECT_CALL(observer, DeviceAdded(&discovery, device0_raw));
  EXPECT_CALL(*device0, GetId()).WillOnce(Return("device0"));
  EXPECT_TRUE(discovery.AddDevice(std::move(device0)));

  // // Expect successful insertion.
  auto device1 = std::make_unique<MockU2fDevice>();
  auto* device1_raw = device1.get();
  EXPECT_CALL(observer, DeviceAdded(&discovery, device1_raw));
  EXPECT_CALL(*device1, GetId()).WillOnce(Return("device1"));
  EXPECT_TRUE(discovery.AddDevice(std::move(device1)));

  // Inserting a device with an already present id should be prevented.
  auto device1_dup = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(observer, DeviceAdded(_, _)).Times(0);
  EXPECT_CALL(*device1_dup, GetId()).WillOnce(Return("device1"));
  EXPECT_FALSE(discovery.AddDevice(std::move(device1_dup)));

  EXPECT_EQ(device0_raw, discovery.GetDevice("device0"));
  EXPECT_EQ(device1_raw, discovery.GetDevice("device1"));
  EXPECT_THAT(discovery.GetDevices(),
              UnorderedElementsAre(device0_raw, device1_raw));

  const U2fDiscovery& const_discovery = discovery;
  EXPECT_EQ(device0_raw, const_discovery.GetDevice("device0"));
  EXPECT_EQ(device1_raw, const_discovery.GetDevice("device1"));
  EXPECT_THAT(const_discovery.GetDevices(),
              UnorderedElementsAre(device0_raw, device1_raw));

  // Trying to remove a non-present device should fail.
  EXPECT_CALL(observer, DeviceRemoved(_, _)).Times(0);
  EXPECT_FALSE(discovery.RemoveDevice("device2"));

  EXPECT_CALL(observer, DeviceRemoved(&discovery, device1_raw));
  EXPECT_TRUE(discovery.RemoveDevice("device1"));

  EXPECT_CALL(observer, DeviceRemoved(&discovery, device0_raw));
  EXPECT_TRUE(discovery.RemoveDevice("device0"));
}

}  // namespace device
