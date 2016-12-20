// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_advertisement_device_queue.h"

#include <memory>

#include "chromeos/components/tether/ble_constants.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace chromeos {

namespace tether {

class BleAdvertisementDeviceQueueTest : public testing::Test {
 protected:
  BleAdvertisementDeviceQueueTest() : device_queue_(nullptr) {}

  void SetUp() override {
    device_queue_.reset(new BleAdvertisementDeviceQueue());
  }

  std::unique_ptr<BleAdvertisementDeviceQueue> device_queue_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BleAdvertisementDeviceQueueTest);
};

TEST_F(BleAdvertisementDeviceQueueTest, TestEmptyQueue) {
  std::vector<cryptauth::RemoteDevice> to_advertise =
      device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_EQ(static_cast<size_t>(0), device_queue_->GetSize());
  EXPECT_TRUE(to_advertise.empty());
}

TEST_F(BleAdvertisementDeviceQueueTest, TestSingleDevice) {
  std::vector<cryptauth::RemoteDevice> devices =
      cryptauth::GenerateTestRemoteDevices(1);
  EXPECT_TRUE(device_queue_->SetDevices(devices));
  EXPECT_EQ(static_cast<size_t>(1), device_queue_->GetSize());

  std::vector<cryptauth::RemoteDevice> to_advertise =
      device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(devices[0]));
}

TEST_F(BleAdvertisementDeviceQueueTest, TestSingleDevice_MoveToEnd) {
  std::vector<cryptauth::RemoteDevice> devices =
      cryptauth::GenerateTestRemoteDevices(1);
  EXPECT_TRUE(device_queue_->SetDevices(devices));
  EXPECT_EQ(static_cast<size_t>(1), device_queue_->GetSize());

  std::vector<cryptauth::RemoteDevice> to_advertise =
      device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(devices[0]));

  device_queue_->MoveDeviceToEnd(devices[0].GetDeviceId());
  to_advertise = device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(devices[0]));
}

TEST_F(BleAdvertisementDeviceQueueTest, TestTwoDevices) {
  std::vector<cryptauth::RemoteDevice> devices =
      cryptauth::GenerateTestRemoteDevices(2);
  EXPECT_TRUE(device_queue_->SetDevices(devices));
  EXPECT_EQ(static_cast<size_t>(2), device_queue_->GetSize());

  std::vector<cryptauth::RemoteDevice> to_advertise =
      device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(devices[0], devices[1]));
}

TEST_F(BleAdvertisementDeviceQueueTest, TestTwoDevices_MoveToEnd) {
  std::vector<cryptauth::RemoteDevice> devices =
      cryptauth::GenerateTestRemoteDevices(2);
  EXPECT_TRUE(device_queue_->SetDevices(devices));
  EXPECT_EQ(static_cast<size_t>(2), device_queue_->GetSize());

  std::vector<cryptauth::RemoteDevice> to_advertise =
      device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(devices[0], devices[1]));

  device_queue_->MoveDeviceToEnd(devices[0].GetDeviceId());
  to_advertise = device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(devices[1], devices[0]));

  device_queue_->MoveDeviceToEnd(devices[1].GetDeviceId());
  to_advertise = device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(devices[0], devices[1]));
}

TEST_F(BleAdvertisementDeviceQueueTest, TestThreeDevices) {
  // Note: These tests need to be rewritten if |kMaxConcurrentAdvertisements| is
  // ever changed.
  ASSERT_GT(3, kMaxConcurrentAdvertisements);

  std::vector<cryptauth::RemoteDevice> devices =
      cryptauth::GenerateTestRemoteDevices(3);
  EXPECT_TRUE(device_queue_->SetDevices(devices));
  EXPECT_EQ(static_cast<size_t>(3), device_queue_->GetSize());

  std::vector<cryptauth::RemoteDevice> to_advertise =
      device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(devices[0], devices[1]));

  device_queue_->MoveDeviceToEnd(devices[0].GetDeviceId());
  to_advertise = device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(devices[1], devices[2]));

  device_queue_->MoveDeviceToEnd(devices[1].GetDeviceId());
  to_advertise = device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(devices[2], devices[0]));

  device_queue_->MoveDeviceToEnd(devices[2].GetDeviceId());
  to_advertise = device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(devices[0], devices[1]));
}

TEST_F(BleAdvertisementDeviceQueueTest, TestAddingDevices) {
  // Note: These tests need to be rewritten if |kMaxConcurrentAdvertisements| is
  // ever changed.
  ASSERT_GT(3, kMaxConcurrentAdvertisements);

  std::vector<cryptauth::RemoteDevice> all_devices =
      cryptauth::GenerateTestRemoteDevices(5);
  std::vector<cryptauth::RemoteDevice> initial_devices = {all_devices[0],
                                                          all_devices[1]};
  std::vector<cryptauth::RemoteDevice> updated_devices_list = {
      all_devices[1], all_devices[2], all_devices[3], all_devices[4]};

  EXPECT_TRUE(device_queue_->SetDevices(initial_devices));
  EXPECT_EQ(static_cast<size_t>(2), device_queue_->GetSize());

  std::vector<cryptauth::RemoteDevice> to_advertise =
      device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(all_devices[0], all_devices[1]));

  device_queue_->MoveDeviceToEnd(all_devices[0].GetDeviceId());
  to_advertise = device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(all_devices[1], all_devices[0]));

  // Device #1 has been unregistered; Devices #3 and #4 have been registered.
  EXPECT_TRUE(device_queue_->SetDevices(updated_devices_list));
  EXPECT_EQ(static_cast<size_t>(4), device_queue_->GetSize());
  to_advertise = device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(all_devices[1], all_devices[2]));

  device_queue_->MoveDeviceToEnd(all_devices[2].GetDeviceId());
  to_advertise = device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(all_devices[1], all_devices[3]));

  device_queue_->MoveDeviceToEnd(all_devices[1].GetDeviceId());
  to_advertise = device_queue_->GetDevicesToWhichToAdvertise();
  EXPECT_THAT(to_advertise, ElementsAre(all_devices[3], all_devices[4]));
}

TEST_F(BleAdvertisementDeviceQueueTest, TestSettingSameDevices) {
  std::vector<cryptauth::RemoteDevice> devices =
      cryptauth::GenerateTestRemoteDevices(2);
  EXPECT_TRUE(device_queue_->SetDevices(devices));
  EXPECT_FALSE(device_queue_->SetDevices(devices));
  EXPECT_FALSE(device_queue_->SetDevices(devices));
  EXPECT_TRUE(
      device_queue_->SetDevices(cryptauth::GenerateTestRemoteDevices(3)));
}

}  // namespace tether

}  // namespace cryptauth
