// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/sync/glue/local_device_info_provider_impl.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/sync_util.h"
#include "content/public/browser/browser_thread.h"
#include "sync/util/get_session_name.h"
#include "ui/base/device_form_factor.h"

namespace browser_sync {

namespace {

#if defined(OS_ANDROID)
bool IsTabletUI() {
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
}
#endif

sync_pb::SyncEnums::DeviceType GetLocalDeviceType() {
#if defined(OS_CHROMEOS)
  return sync_pb::SyncEnums_DeviceType_TYPE_CROS;
#elif defined(OS_LINUX)
  return sync_pb::SyncEnums_DeviceType_TYPE_LINUX;
#elif defined(OS_MACOSX)
  return sync_pb::SyncEnums_DeviceType_TYPE_MAC;
#elif defined(OS_WIN)
  return sync_pb::SyncEnums_DeviceType_TYPE_WIN;
#elif defined(OS_ANDROID)
  return IsTabletUI() ? sync_pb::SyncEnums_DeviceType_TYPE_TABLET
                      : sync_pb::SyncEnums_DeviceType_TYPE_PHONE;
#else
  return sync_pb::SyncEnums_DeviceType_TYPE_OTHER;
#endif
}

}  // namespace

LocalDeviceInfoProviderImpl::LocalDeviceInfoProviderImpl()
    : weak_factory_(this) {
}

LocalDeviceInfoProviderImpl::~LocalDeviceInfoProviderImpl() {
}

const sync_driver::DeviceInfo*
LocalDeviceInfoProviderImpl::GetLocalDeviceInfo() const {
  return local_device_info_.get();
}

std::string LocalDeviceInfoProviderImpl::GetSyncUserAgent() const {
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  return MakeDesktopUserAgentForSync();
#elif defined(OS_CHROMEOS)
  return MakeUserAgentForSync("CROS ");
#elif defined(OS_ANDROID)
  if (IsTabletUI()) {
    return MakeUserAgentForSync("ANDROID-TABLET ");
  } else {
    return MakeUserAgentForSync("ANDROID-PHONE ");
  }
#endif
}

std::string LocalDeviceInfoProviderImpl::GetLocalSyncCacheGUID() const {
  return cache_guid_;
}

scoped_ptr<sync_driver::LocalDeviceInfoProvider::Subscription>
LocalDeviceInfoProviderImpl::RegisterOnInitializedCallback(
    const base::Closure& callback) {
  DCHECK(!local_device_info_.get());
  return callback_list_.Add(callback);
}

void LocalDeviceInfoProviderImpl::Initialize(
    const std::string& cache_guid, const std::string& signin_scoped_device_id) {
  DCHECK(!cache_guid.empty());
  cache_guid_ = cache_guid;

  syncer::GetSessionName(
      content::BrowserThread::GetBlockingPool(),
      base::Bind(&LocalDeviceInfoProviderImpl::InitializeContinuation,
                 weak_factory_.GetWeakPtr(),
                 cache_guid,
                 signin_scoped_device_id));
}

void LocalDeviceInfoProviderImpl::InitializeContinuation(
    const std::string& guid,
    const std::string& signin_scoped_device_id,
    const std::string& session_name) {
  local_device_info_.reset(
      new sync_driver::DeviceInfo(guid,
                                  session_name,
                                  chrome::GetVersionString(),
                                  GetSyncUserAgent(),
                                  GetLocalDeviceType(),
                                  signin_scoped_device_id));

  // Notify observers.
  callback_list_.Notify();
}

}  // namespace browser_sync
