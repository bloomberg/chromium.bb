// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/device_info_service.h"

#include <string>

#include "components/sync_driver/local_device_info_provider_mock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_driver_v2 {

using sync_driver::DeviceInfo;
using sync_driver::DeviceInfoTracker;
using sync_driver::LocalDeviceInfoProviderMock;

namespace {

class DeviceInfoServiceTest : public testing::Test,
                              public DeviceInfoTracker::Observer {
 protected:
  DeviceInfoServiceTest() : num_device_info_changed_callbacks_(0) {}

  void InitFully() {
    local_device_.reset(new LocalDeviceInfoProviderMock(
        "guid_1", "client_1", "Chromium 10k", "Chrome 10k",
        sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id"));
    service_.reset(new DeviceInfoService(local_device_.get()));
    service_->AddObserver(this);
  }

  void InitPartially() {
    local_device_.reset(new LocalDeviceInfoProviderMock());
    service_.reset(new DeviceInfoService(local_device_.get()));
    service_->AddObserver(this);
  }

  ~DeviceInfoServiceTest() override { service_->RemoveObserver(this); }

  void OnDeviceInfoChange() override { num_device_info_changed_callbacks_++; }

 protected:
  int num_device_info_changed_callbacks_;
  scoped_ptr<LocalDeviceInfoProviderMock> local_device_;
  scoped_ptr<DeviceInfoService> service_;
};

TEST_F(DeviceInfoServiceTest, StartSyncEmptyInitialData) {
  InitFully();
  EXPECT_FALSE(service_->IsSyncing());
}

TEST_F(DeviceInfoServiceTest, DelayedProviderInitialization) {
  InitPartially();
  local_device_->Initialize(make_scoped_ptr(
      new DeviceInfo("guid_1", "client_1", "Chromium 10k", "Chrome 10k",
                     sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id")));

  EXPECT_FALSE(service_->IsSyncing());
}

}  // namespace

}  // namespace sync_driver_v2
