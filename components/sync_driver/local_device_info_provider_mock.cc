// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/local_device_info_provider_mock.h"

namespace sync_driver {

LocalDeviceInfoProviderMock::LocalDeviceInfoProviderMock()
  : is_initialized_(false) {}

LocalDeviceInfoProviderMock::LocalDeviceInfoProviderMock(
    const std::string& guid,
    const std::string& client_name,
    const std::string& chrome_version,
    const std::string& sync_user_agent,
    const sync_pb::SyncEnums::DeviceType device_type,
    const std::string& signin_scoped_device_id)
  : is_initialized_(true) {
  local_device_info_.reset(
      new DeviceInfo(
          guid,
          client_name,
          chrome_version,
          sync_user_agent,
          device_type,
          signin_scoped_device_id));
}

LocalDeviceInfoProviderMock::~LocalDeviceInfoProviderMock() {}

const DeviceInfo* LocalDeviceInfoProviderMock::GetLocalDeviceInfo() const {
  return is_initialized_ ? local_device_info_.get() : NULL;
}

std::string LocalDeviceInfoProviderMock::GetSyncUserAgent() const {
  return "useragent";
}

std::string LocalDeviceInfoProviderMock::GetLocalSyncCacheGUID() const {
  return local_device_info_.get() ? local_device_info_->guid() : "";
}

void LocalDeviceInfoProviderMock::Initialize(
    const std::string& cache_guid,
    const std::string& signin_scoped_device_id,
    const scoped_refptr<base::TaskRunner>& blocking_task_runner) {
  // Ignored for the mock provider.
}

void LocalDeviceInfoProviderMock::Initialize(
    std::unique_ptr<DeviceInfo> local_device_info) {
  local_device_info_.swap(local_device_info);
  SetInitialized(true);
}

std::unique_ptr<LocalDeviceInfoProvider::Subscription>
LocalDeviceInfoProviderMock::RegisterOnInitializedCallback(
    const base::Closure& callback) {
  DCHECK(!is_initialized_);
  return callback_list_.Add(callback);
}

void LocalDeviceInfoProviderMock::SetInitialized(bool is_initialized) {
  is_initialized_ = is_initialized;
  if (is_initialized_) {
    callback_list_.Notify();
  }
}

}  // namespace sync_driver
