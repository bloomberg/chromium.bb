// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_hid_discovery.h"

#include <string>
#include <utility>

#include "base/test/scoped_task_environment.h"
#include "device/hid/hid_collection_info.h"
#include "device/hid/public/interfaces/hid.mojom.h"
#include "device/u2f/fake_hid_impl_for_testing.h"
#include "device/u2f/mock_u2f_discovery.h"
#include "device/u2f/u2f_hid_device.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

device::mojom::HidDeviceInfoPtr MakeU2fDevice(std::string guid) {
  HidCollectionInfo c_info;
  c_info.usage = HidUsageAndPage(1, static_cast<HidUsageAndPage::Page>(0xf1d0));

  auto u2f_device = device::mojom::HidDeviceInfo::New();
  u2f_device->guid = std::move(guid);
  u2f_device->product_name = "Test Fido Device";
  u2f_device->serial_number = "123FIDO";
  u2f_device->bus_type = device::mojom::HidBusType::kHIDBusTypeUSB;
  u2f_device->collections.push_back(c_info);
  u2f_device->max_input_report_size = 64;
  u2f_device->max_output_report_size = 64;
  return u2f_device;
}

device::mojom::HidDeviceInfoPtr MakeOtherDevice(std::string guid) {
  auto other_device = device::mojom::HidDeviceInfo::New();
  other_device->guid = std::move(guid);
  other_device->product_name = "Other Device";
  other_device->serial_number = "OtherDevice";
  other_device->bus_type = device::mojom::HidBusType::kHIDBusTypeUSB;
  return other_device;
}

}  // namespace

class U2fHidDiscoveryTest : public testing::Test {
 public:
  base::test::ScopedTaskEnvironment& scoped_task_environment() {
    return scoped_task_environment_;
  }

  void SetUp() override {
    fake_hid_manager_ = std::make_unique<FakeHidManager>();

    service_manager::mojom::ConnectorRequest request;
    connector_ = service_manager::Connector::Create(&request);
    service_manager::Connector::TestApi test_api(connector_.get());
    test_api.OverrideBinderForTesting(
        device::mojom::kServiceName, device::mojom::HidManager::Name_,
        base::Bind(&FakeHidManager::AddBinding,
                   base::Unretained(fake_hid_manager_.get())));
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<service_manager::Connector> connector_;
  std::unique_ptr<FakeHidManager> fake_hid_manager_;
};

TEST_F(U2fHidDiscoveryTest, TestAddRemoveDevice) {
  U2fHidDiscovery discovery(connector_.get());
  MockU2fDiscovery::MockDelegate delegate;

  auto u2f_device_known = MakeU2fDevice("known");
  std::string device_id_known =
      U2fHidDevice(u2f_device_known->Clone(), nullptr).GetId();
  fake_hid_manager_->AddDevice(std::move(u2f_device_known));

  EXPECT_CALL(delegate, OnStarted(true));
  discovery.SetDelegate(delegate.GetWeakPtr());
  discovery.Start();

  // Devices initially known to the service before discovery started should be
  // reported as KNOWN.
  EXPECT_CALL(delegate, OnDeviceAddedStr(device_id_known));
  scoped_task_environment().RunUntilIdle();

  // Devices added during the discovery should be reported as ADDED.
  auto u2f_device_added = MakeU2fDevice("added");
  std::string device_id_added =
      U2fHidDevice(u2f_device_added->Clone(), nullptr).GetId();
  EXPECT_CALL(delegate, OnDeviceAddedStr(device_id_added));
  fake_hid_manager_->AddDevice(std::move(u2f_device_added));
  scoped_task_environment().RunUntilIdle();

  // Added non-U2F devices should not be reported at all.
  auto other_device = MakeOtherDevice("other");
  EXPECT_CALL(delegate, OnDeviceAddedStr(testing::_)).Times(0);
  fake_hid_manager_->AddDevice(std::move(other_device));

  // Removed non-U2F devices should not be reported at all.
  EXPECT_CALL(delegate, OnDeviceRemovedStr(testing::_)).Times(0);
  fake_hid_manager_->RemoveDevice("other");
  scoped_task_environment().RunUntilIdle();

  // Removed U2F devices should be reported as REMOVED.
  EXPECT_CALL(delegate, OnDeviceRemovedStr(device_id_known));
  EXPECT_CALL(delegate, OnDeviceRemovedStr(device_id_added));
  fake_hid_manager_->RemoveDevice("known");
  fake_hid_manager_->RemoveDevice("added");
  scoped_task_environment().RunUntilIdle();
}

}  // namespace device
