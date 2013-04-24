// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"

#include <string>
#include <utility>

#include "base/stl_util.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"
#include "webkit/fileapi/isolated_context.h"

namespace chrome {

namespace {

base::LazyInstance<MTPDeviceMapService> g_mtp_device_map_service =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
MTPDeviceMapService* MTPDeviceMapService::GetInstance() {
  return g_mtp_device_map_service.Pointer();
}

/////////////////////////////////////////////////////////////////////////////
//   Following methods are used to manage MTPDeviceAsyncDelegate objects.  //
/////////////////////////////////////////////////////////////////////////////
void MTPDeviceMapService::AddAsyncDelegate(
    const base::FilePath::StringType& device_location,
    MTPDeviceAsyncDelegate* delegate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(delegate);
  DCHECK(!device_location.empty());
  if (ContainsKey(async_delegate_map_, device_location))
    return;
  async_delegate_map_[device_location] = delegate;
}

void MTPDeviceMapService::RemoveAsyncDelegate(
    const base::FilePath::StringType& device_location) {
  DCHECK(thread_checker_.CalledOnValidThread());
  AsyncDelegateMap::iterator it = async_delegate_map_.find(device_location);
  DCHECK(it != async_delegate_map_.end());
  it->second->CancelPendingTasksAndDeleteDelegate();
  async_delegate_map_.erase(it);
}

MTPDeviceAsyncDelegate* MTPDeviceMapService::GetMTPDeviceAsyncDelegate(
    const std::string& filesystem_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::FilePath device_path;
  if (!fileapi::IsolatedContext::GetInstance()->GetRegisteredPath(
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

MTPDeviceMapService::~MTPDeviceMapService() {}

}  // namespace chrome
