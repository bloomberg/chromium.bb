// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/local_device_info_provider_impl.h"

#include "base/bind.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "components/sync_driver/sync_util.h"
#include "sync/util/get_session_name.h"

namespace browser_sync {

namespace {

sync_pb::SyncEnums::DeviceType GetLocalDeviceType(bool is_tablet) {
#if defined(OS_CHROMEOS)
  return sync_pb::SyncEnums_DeviceType_TYPE_CROS;
#elif defined(OS_LINUX)
  return sync_pb::SyncEnums_DeviceType_TYPE_LINUX;
#elif defined(OS_ANDROID) || defined(OS_IOS)
  return is_tablet ? sync_pb::SyncEnums_DeviceType_TYPE_TABLET
                   : sync_pb::SyncEnums_DeviceType_TYPE_PHONE;
#elif defined(OS_MACOSX)
  return sync_pb::SyncEnums_DeviceType_TYPE_MAC;
#elif defined(OS_WIN)
  return sync_pb::SyncEnums_DeviceType_TYPE_WIN;
#else
  return sync_pb::SyncEnums_DeviceType_TYPE_OTHER;
#endif
}

}  // namespace

LocalDeviceInfoProviderImpl::LocalDeviceInfoProviderImpl(
    version_info::Channel channel,
    const std::string& version,
    bool is_tablet)
    : channel_(channel),
      version_(version),
      is_tablet_(is_tablet),
      weak_factory_(this) {
  DCHECK(CalledOnValidThread());
}

LocalDeviceInfoProviderImpl::~LocalDeviceInfoProviderImpl() {}

const sync_driver::DeviceInfo* LocalDeviceInfoProviderImpl::GetLocalDeviceInfo()
    const {
  DCHECK(CalledOnValidThread());
  return local_device_info_.get();
}

std::string LocalDeviceInfoProviderImpl::GetSyncUserAgent() const {
  DCHECK(CalledOnValidThread());
#if defined(OS_CHROMEOS)
  return MakeUserAgentForSync("CROS ", channel_);
#elif defined(OS_ANDROID)
  if (is_tablet_) {
    return MakeUserAgentForSync("ANDROID-TABLET ", channel_);
  } else {
    return MakeUserAgentForSync("ANDROID-PHONE ", channel_);
  }
#elif defined(OS_IOS)
  if (is_tablet_) {
    return MakeUserAgentForSync("IOS-TABLET ", channel_);
  } else {
    return MakeUserAgentForSync("IOS-PHONE ", channel_);
  }
#else
  return MakeDesktopUserAgentForSync(channel_);
#endif
}

std::string LocalDeviceInfoProviderImpl::GetLocalSyncCacheGUID() const {
  DCHECK(CalledOnValidThread());
  return cache_guid_;
}

std::unique_ptr<sync_driver::LocalDeviceInfoProvider::Subscription>
LocalDeviceInfoProviderImpl::RegisterOnInitializedCallback(
    const base::Closure& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!local_device_info_.get());
  return callback_list_.Add(callback);
}

void LocalDeviceInfoProviderImpl::Initialize(
    const std::string& cache_guid,
    const std::string& signin_scoped_device_id,
    const scoped_refptr<base::TaskRunner>& blocking_task_runner) {
  DCHECK(CalledOnValidThread());
  DCHECK(!cache_guid.empty());
  cache_guid_ = cache_guid;

  syncer::GetSessionName(
      blocking_task_runner,
      base::Bind(&LocalDeviceInfoProviderImpl::InitializeContinuation,
                 weak_factory_.GetWeakPtr(), cache_guid,
                 signin_scoped_device_id));
}

void LocalDeviceInfoProviderImpl::InitializeContinuation(
    const std::string& guid,
    const std::string& signin_scoped_device_id,
    const std::string& session_name) {
  DCHECK(CalledOnValidThread());
  if (guid != cache_guid_) {
    // Clear() happened before this callback; abort.
    return;
  }

  local_device_info_.reset(new sync_driver::DeviceInfo(
      guid, session_name, version_, GetSyncUserAgent(),
      GetLocalDeviceType(is_tablet_), signin_scoped_device_id));

  // Notify observers.
  callback_list_.Notify();
}

void LocalDeviceInfoProviderImpl::Clear() {
  DCHECK(CalledOnValidThread());
  cache_guid_ = "";
  local_device_info_.reset();
}

}  // namespace browser_sync
