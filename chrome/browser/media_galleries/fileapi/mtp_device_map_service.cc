// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"

#include <string>
#include <utility>

#include "base/stl_util.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/fileapi/external_mount_points.h"

namespace {

base::LazyInstance<MTPDeviceMapService> g_mtp_device_map_service =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
MTPDeviceMapService* MTPDeviceMapService::GetInstance() {
  return g_mtp_device_map_service.Pointer();
}

void MTPDeviceMapService::RegisterMTPFileSystem(
    const base::FilePath::StringType& device_location,
    const std::string& fsid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!ContainsKey(mtp_device_usage_map_, device_location)) {
    // Note that this initializes the delegate asynchronously, but since
    // the delegate will only be used from the IO thread, it is guaranteed
    // to be created before use of it expects it to be there.
    CreateMTPDeviceAsyncDelegate(device_location,
        base::Bind(&MTPDeviceMapService::AddAsyncDelegate,
                   base::Unretained(this), device_location));
    mtp_device_usage_map_[device_location] = 0;
  }

  mtp_device_usage_map_[device_location]++;
  mtp_device_map_[fsid] = device_location;
}

void MTPDeviceMapService::RevokeMTPFileSystem(const std::string& fsid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  MTPDeviceFileSystemMap::iterator it = mtp_device_map_.find(fsid);
  if (it != mtp_device_map_.end()) {
    base::FilePath::StringType device_location = it->second;
    mtp_device_map_.erase(it);
    MTPDeviceUsageMap::iterator delegate_it =
        mtp_device_usage_map_.find(device_location);
    DCHECK(delegate_it != mtp_device_usage_map_.end());
    mtp_device_usage_map_[device_location]--;
    if (mtp_device_usage_map_[device_location] == 0) {
      mtp_device_usage_map_.erase(delegate_it);
      RemoveAsyncDelegate(device_location);
    }
  }
}

void MTPDeviceMapService::AddAsyncDelegate(
    const base::FilePath::StringType& device_location,
    MTPDeviceAsyncDelegate* delegate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(delegate);
  DCHECK(!device_location.empty());
  if (ContainsKey(async_delegate_map_, device_location))
    return;
  async_delegate_map_[device_location] = delegate;
}

void MTPDeviceMapService::RemoveAsyncDelegate(
    const base::FilePath::StringType& device_location) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  AsyncDelegateMap::iterator it = async_delegate_map_.find(device_location);
  DCHECK(it != async_delegate_map_.end());
  it->second->CancelPendingTasksAndDeleteDelegate();
  async_delegate_map_.erase(it);
}

MTPDeviceAsyncDelegate* MTPDeviceMapService::GetMTPDeviceAsyncDelegate(
    const std::string& filesystem_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::FilePath device_path;
  if (!storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          filesystem_id, &device_path)) {
    return NULL;
  }

  const base::FilePath::StringType& device_location = device_path.value();
  DCHECK(!device_location.empty());
  AsyncDelegateMap::const_iterator it =
      async_delegate_map_.find(device_location);
  return (it != async_delegate_map_.end()) ? it->second : NULL;
}

MTPDeviceMapService::MTPDeviceMapService() {
}

MTPDeviceMapService::~MTPDeviceMapService() {
  DCHECK(mtp_device_usage_map_.empty());
}
