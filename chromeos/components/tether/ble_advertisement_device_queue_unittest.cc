// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_advertisement_device_queue.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chromeos/components/tether/ble_constants.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace chromeos {

namespace tether {

typedef BleAdvertisementDeviceQueue::PrioritizedDevice PrioritizedDevice;

class BleAdvertisementDeviceQueueTest : public testing::Test {
 protected:
  BleAdvertisementDeviceQueueTest()
      : test_devices_(cryptauth::GenerateTestRemoteDevices(5)) {}

  void SetUp() override {
    device_queue_ = base::MakeUnique<BleAdvertisementDeviceQueue>();
  }

  std::unique_ptr<BleAdvertisementDeviceQueue> device_queue_;

  const std::vector<cryptauth::RemoteDevice> test_devices_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BleAdvertisementDeviceQueueTest);
};

TEST_F(BleAdvertisementDeviceQueueTest, TestEmptyQueue) {
  EXPECT_EQ(0u, device_queue_->GetSize());
  std::vector<cryptauth::RemoteDevice> to_advertise =
      device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_TRUE(to_advertise.empty());
}

TEST_F(BleAdvertisementDeviceQueueTest, TestSingleDevice) {
  EXPECT_TRUE(device_queue_->SetDevices({PrioritizedDevice(
      test_devices_[0], ConnectionPriority::CONNECTION_PRIORITY_HIGH)}));
  EXPECT_EQ(1u, device_queue_->GetSize());

  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[0]));
}

TEST_F(BleAdvertisementDeviceQueueTest, TestSingleDevice_MoveToEnd) {
  EXPECT_TRUE(device_queue_->SetDevices({PrioritizedDevice(
      test_devices_[0], ConnectionPriority::CONNECTION_PRIORITY_HIGH)}));
  EXPECT_EQ(1u, device_queue_->GetSize());

  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[0]));

  device_queue_->MoveDeviceToEnd(test_devices_[0].GetDeviceId());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[0]));
}

TEST_F(BleAdvertisementDeviceQueueTest, TestTwoDevices) {
  EXPECT_TRUE(device_queue_->SetDevices(
      {PrioritizedDevice(test_devices_[0],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH),
       PrioritizedDevice(test_devices_[1],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH)}));
  EXPECT_EQ(2u, device_queue_->GetSize());

  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[0], test_devices_[1]));
}

TEST_F(BleAdvertisementDeviceQueueTest, TestTwoDevices_MoveToEnd) {
  EXPECT_TRUE(device_queue_->SetDevices(
      {PrioritizedDevice(test_devices_[0],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH),
       PrioritizedDevice(test_devices_[1],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH)}));
  EXPECT_EQ(2u, device_queue_->GetSize());

  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[0], test_devices_[1]));

  device_queue_->MoveDeviceToEnd(test_devices_[0].GetDeviceId());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[1], test_devices_[0]));

  device_queue_->MoveDeviceToEnd(test_devices_[1].GetDeviceId());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[0], test_devices_[1]));
}

TEST_F(BleAdvertisementDeviceQueueTest, TestThreeDevices) {
  // Note: These tests need to be rewritten if |kMaxConcurrentAdvertisements| is
  // ever changed.
  ASSERT_GT(3u, kMaxConcurrentAdvertisements);

  EXPECT_TRUE(device_queue_->SetDevices(
      {PrioritizedDevice(test_devices_[0],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH),
       PrioritizedDevice(test_devices_[1],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH),
       PrioritizedDevice(test_devices_[2],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH)}));
  EXPECT_EQ(3u, device_queue_->GetSize());

  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[0], test_devices_[1]));

  device_queue_->MoveDeviceToEnd(test_devices_[0].GetDeviceId());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[1], test_devices_[2]));

  device_queue_->MoveDeviceToEnd(test_devices_[1].GetDeviceId());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[2], test_devices_[0]));

  device_queue_->MoveDeviceToEnd(test_devices_[2].GetDeviceId());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[0], test_devices_[1]));
}

TEST_F(BleAdvertisementDeviceQueueTest, TestAddingDevices) {
  // Note: These tests need to be rewritten if |kMaxConcurrentAdvertisements| is
  // ever changed.
  ASSERT_GT(3u, kMaxConcurrentAdvertisements);

  EXPECT_TRUE(device_queue_->SetDevices(
      {PrioritizedDevice(test_devices_[0],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH),
       PrioritizedDevice(test_devices_[1],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH)}));
  EXPECT_EQ(2u, device_queue_->GetSize());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[0], test_devices_[1]));

  device_queue_->MoveDeviceToEnd(test_devices_[0].GetDeviceId());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[1], test_devices_[0]));

  // Device 0 has been unregistered; devices 3 and 4 have been registered.
  EXPECT_TRUE(device_queue_->SetDevices(
      {PrioritizedDevice(test_devices_[1],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH),
       PrioritizedDevice(test_devices_[2],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH),
       PrioritizedDevice(test_devices_[3],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH),
       PrioritizedDevice(test_devices_[4],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH)}));
  EXPECT_EQ(4u, device_queue_->GetSize());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[1], test_devices_[2]));

  device_queue_->MoveDeviceToEnd(test_devices_[2].GetDeviceId());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[1], test_devices_[3]));

  device_queue_->MoveDeviceToEnd(test_devices_[1].GetDeviceId());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[3], test_devices_[4]));
}

TEST_F(BleAdvertisementDeviceQueueTest, TestMultiplePriorityLevels) {
  // Note: These tests need to be rewritten if |kMaxConcurrentAdvertisements| is
  // ever changed.
  ASSERT_GT(3u, kMaxConcurrentAdvertisements);

  EXPECT_TRUE(device_queue_->SetDevices(
      {PrioritizedDevice(test_devices_[0],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH),
       PrioritizedDevice(test_devices_[1],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH),
       PrioritizedDevice(test_devices_[2],
                         ConnectionPriority::CONNECTION_PRIORITY_MEDIUM),
       PrioritizedDevice(test_devices_[3],
                         ConnectionPriority::CONNECTION_PRIORITY_MEDIUM),
       PrioritizedDevice(test_devices_[4],
                         ConnectionPriority::CONNECTION_PRIORITY_LOW)}));
  EXPECT_EQ(5u, device_queue_->GetSize());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[0], test_devices_[1]));

  // Moving a high-priority device to the end of the queue should still keep it
  // before all other priority levels.
  device_queue_->MoveDeviceToEnd(test_devices_[0].GetDeviceId());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[1], test_devices_[0]));

  // Device 0 has been unregistered; device 2 has moved to low-priority.
  EXPECT_TRUE(device_queue_->SetDevices(
      {PrioritizedDevice(test_devices_[1],
                         ConnectionPriority::CONNECTION_PRIORITY_HIGH),
       PrioritizedDevice(test_devices_[2],
                         ConnectionPriority::CONNECTION_PRIORITY_LOW),
       PrioritizedDevice(test_devices_[3],
                         ConnectionPriority::CONNECTION_PRIORITY_MEDIUM),
       PrioritizedDevice(test_devices_[4],
                         ConnectionPriority::CONNECTION_PRIORITY_LOW)}));
  EXPECT_EQ(4u, device_queue_->GetSize());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[1], test_devices_[3]));

  // Moving a high-priority device to the end of the queue should still keep it
  // before all other priority levels.
  device_queue_->MoveDeviceToEnd(test_devices_[1].GetDeviceId());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[1], test_devices_[3]));

  // Likewise, moving a medium-priority device to the end of the queue should
  // still keep it before all low-priority devices.
  device_queue_->MoveDeviceToEnd(test_devices_[3].GetDeviceId());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[1], test_devices_[3]));

  // Now, all devices are priority medium. Since device 3 was already at the
  // medium priority, it should still be in front of the other devices that
  // were just added.
  EXPECT_TRUE(device_queue_->SetDevices(
      {PrioritizedDevice(test_devices_[1],
                         ConnectionPriority::CONNECTION_PRIORITY_MEDIUM),
       PrioritizedDevice(test_devices_[2],
                         ConnectionPriority::CONNECTION_PRIORITY_MEDIUM),
       PrioritizedDevice(test_devices_[3],
                         ConnectionPriority::CONNECTION_PRIORITY_MEDIUM),
       PrioritizedDevice(test_devices_[4],
                         ConnectionPriority::CONNECTION_PRIORITY_MEDIUM)}));
  EXPECT_EQ(4u, device_queue_->GetSize());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[3], test_devices_[1]));

  // Since all devices are the same priority, moving one to the end of the queue
  // should bring a new one to the front.
  device_queue_->MoveDeviceToEnd(test_devices_[1].GetDeviceId());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[3], test_devices_[2]));

  // Leave only one low-priority device left.
  EXPECT_TRUE(device_queue_->SetDevices({PrioritizedDevice(
      test_devices_[1], ConnectionPriority::CONNECTION_PRIORITY_LOW)}));
  EXPECT_EQ(1u, device_queue_->GetSize());
  EXPECT_THAT(device_queue_->GetDevicesToWhichToAdvertise(),
              ElementsAre(test_devices_[1]));

  // Empty the queue.
  EXPECT_TRUE(device_queue_->SetDevices(std::vector<PrioritizedDevice>()));
  EXPECT_EQ(0u, device_queue_->GetSize());
  EXPECT_TRUE(device_queue_->GetDevicesToWhichToAdvertise().empty());
}

TEST_F(BleAdvertisementDeviceQueueTest, TestSettingSameDevices) {
  std::vector<PrioritizedDevice> prioritized_devices = {PrioritizedDevice(
      test_devices_[0], ConnectionPriority::CONNECTION_PRIORITY_HIGH)};
  EXPECT_TRUE(device_queue_->SetDevices(prioritized_devices));

  // Setting the same devices again should return false.
  EXPECT_FALSE(device_queue_->SetDevices(prioritized_devices));
  EXPECT_FALSE(device_queue_->SetDevices(prioritized_devices));

  prioritized_devices.push_back(PrioritizedDevice(
      test_devices_[1], ConnectionPriority::CONNECTION_PRIORITY_HIGH));
  EXPECT_TRUE(device_queue_->SetDevices(prioritized_devices));
}

}  // namespace tether

}  // namespace cryptauth
