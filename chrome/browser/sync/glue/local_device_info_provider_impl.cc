// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/local_device_info_provider_impl.h"

namespace browser_sync {

LocalDeviceInfoProviderImpl::LocalDeviceInfoProviderImpl()
    : weak_factory_(this) {
}

LocalDeviceInfoProviderImpl::~LocalDeviceInfoProviderImpl() {
}

const DeviceInfo*
LocalDeviceInfoProviderImpl::GetLocalDeviceInfo() const {
  return local_device_info_.get();
}

std::string LocalDeviceInfoProviderImpl::GetLocalSyncCacheGUID() const {
  return cache_guid_;
}

scoped_ptr<LocalDeviceInfoProvider::Subscription>
LocalDeviceInfoProviderImpl::RegisterOnInitializedCallback(
    const base::Closure& callback) {
  DCHECK(!local_device_info_.get());
  return callback_list_.Add(callback);
}

void LocalDeviceInfoProviderImpl::Initialize(
    const std::string& cache_guid, const std::string& signin_scoped_device_id) {
  DCHECK(!cache_guid.empty());
  cache_guid_ = cache_guid;
  DeviceInfo::CreateLocalDeviceInfo(
      cache_guid_,
      signin_scoped_device_id,
      base::Bind(&LocalDeviceInfoProviderImpl::InitializeContinuation,
                 weak_factory_.GetWeakPtr()));
}

void LocalDeviceInfoProviderImpl::InitializeContinuation(
    const DeviceInfo& local_info) {
  // Copy constructor is disallowed in DeviceInfo, construct a new one from
  // the fields passed in local_info.
  local_device_info_.reset(
      new DeviceInfo(
          local_info.guid(),
          local_info.client_name(),
          local_info.chrome_version(),
          local_info.sync_user_agent(),
          local_info.device_type(),
          local_info.signin_scoped_device_id()));

  // Notify observers.
  callback_list_.Notify();
}

}  // namespace browser_sync

