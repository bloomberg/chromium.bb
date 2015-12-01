// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/device_info_service.h"

#include <string>

#include "components/sync_driver/local_device_info_provider_mock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_driver_v2 {

using syncer_v2::EntityData;
using sync_driver::DeviceInfo;
using sync_driver::DeviceInfoTracker;
using sync_driver::LocalDeviceInfoProviderMock;
using sync_pb::EntitySpecifics;

namespace {

class DeviceInfoServiceTest : public testing::Test,
                              public DeviceInfoTracker::Observer {
 protected:
  ~DeviceInfoServiceTest() override { service_->RemoveObserver(this); }

  void OnDeviceInfoChange() override { num_device_info_changed_callbacks_++; }

 protected:
  DeviceInfoServiceTest() : num_device_info_changed_callbacks_(0) {}

  DeviceInfoService* InitFully() {
    local_device_.reset(new LocalDeviceInfoProviderMock(
        "guid_1", "client_1", "Chromium 10k", "Chrome 10k",
        sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id"));
    service_.reset(new DeviceInfoService(local_device_.get()));
    service_->AddObserver(this);
    return service_.get();
  }

  DeviceInfoService* InitPartially() {
    local_device_.reset(new LocalDeviceInfoProviderMock());
    service_.reset(new DeviceInfoService(local_device_.get()));
    service_->AddObserver(this);
    return service_.get();
  }

  int num_device_info_changed_callbacks() {
    return num_device_info_changed_callbacks_;
  }
  LocalDeviceInfoProviderMock* local_device() { return local_device_.get(); }

 private:
  int num_device_info_changed_callbacks_;
  scoped_ptr<LocalDeviceInfoProviderMock> local_device_;
  scoped_ptr<DeviceInfoService> service_;
};

TEST_F(DeviceInfoServiceTest, StartSyncEmptyInitialData) {
  DeviceInfoService* service = InitFully();
  EXPECT_FALSE(service->IsSyncing());
}

TEST_F(DeviceInfoServiceTest, DelayedProviderInitialization) {
  DeviceInfoService* service = InitPartially();
  local_device()->Initialize(make_scoped_ptr(
      new DeviceInfo("guid_1", "client_1", "Chromium 10k", "Chrome 10k",
                     sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id")));
  EXPECT_FALSE(service->IsSyncing());
}

TEST_F(DeviceInfoServiceTest, GetClientTagNormal) {
  DeviceInfoService* service = InitFully();
  const std::string guid = "abc";
  EntitySpecifics entity_specifics;
  entity_specifics.mutable_device_info()->set_cache_guid(guid);
  EntityData entity_data;
  entity_data.specifics = entity_specifics;
  EXPECT_EQ(guid, service->GetClientTag(entity_data));
}

TEST_F(DeviceInfoServiceTest, GetClientTagEmpty) {
  DeviceInfoService* service = InitFully();
  EntitySpecifics entity_specifics;
  entity_specifics.mutable_device_info();
  EntityData entity_data;
  entity_data.specifics = entity_specifics;
  EXPECT_EQ("", service->GetClientTag(entity_data));
}

}  // namespace

}  // namespace sync_driver_v2
