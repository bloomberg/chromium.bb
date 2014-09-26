// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/local_device_info_provider_mock.h"

namespace browser_sync {

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
      new sync_driver::DeviceInfo(
          guid,
          client_name,
          chrome_version,
          sync_user_agent,
          device_type,
          signin_scoped_device_id));
}

LocalDeviceInfoProviderMock::~LocalDeviceInfoProviderMock() {}

const sync_driver::DeviceInfo*
LocalDeviceInfoProviderMock::GetLocalDeviceInfo() const {
  return is_initialized_ ? local_device_info_.get() : NULL;
}

std::string LocalDeviceInfoProviderMock::GetLocalSyncCacheGUID() const {
  return local_device_info_.get() ? local_device_info_->guid() : "";
}

void LocalDeviceInfoProviderMock::Initialize(
    const std::string& cache_guid, const std::string& signin_scoped_device_id) {
  // Ignored for the mock provider.
}

scoped_ptr<sync_driver::LocalDeviceInfoProvider::Subscription>
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

}  // namespace browser_sync

