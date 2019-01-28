// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/device_info/local_device_info_provider_mock.h"


namespace syncer {

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
  local_device_info_ = std::make_unique<DeviceInfo>(
      guid, client_name, chrome_version, sync_user_agent, device_type,
      signin_scoped_device_id);
}

LocalDeviceInfoProviderMock::~LocalDeviceInfoProviderMock() {}

version_info::Channel LocalDeviceInfoProviderMock::GetChannel() const {
  return version_info::Channel::UNKNOWN;
}

const DeviceInfo* LocalDeviceInfoProviderMock::GetLocalDeviceInfo() const {
  return is_initialized_ ? local_device_info_.get() : nullptr;
}

std::string LocalDeviceInfoProviderMock::GetSyncUserAgent() const {
  return "useragent";
}

void LocalDeviceInfoProviderMock::Initialize(const std::string& cache_guid,
                                             const std::string& session_name) {
  local_device_info_ = std::make_unique<DeviceInfo>(
      cache_guid, session_name, "chrome_version", GetSyncUserAgent(),
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "signin-scoped-device-id");
  SetInitialized(true);
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

void LocalDeviceInfoProviderMock::Clear() {
  local_device_info_.reset();
  is_initialized_ = false;
}

void LocalDeviceInfoProviderMock::SetInitialized(bool is_initialized) {
  is_initialized_ = is_initialized;
  if (is_initialized_) {
    callback_list_.Notify();
  }
}

}  // namespace syncer
