// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/scoped_mtp_device_map_entry.h"

#include "webkit/fileapi/media/mtp_device_file_system_config.h"

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
#include "chrome/browser/media_gallery/mtp_device_delegate_impl.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/fileapi/media/mtp_device_map_service.h"
#endif

namespace chrome {

ScopedMTPDeviceMapEntry::ScopedMTPDeviceMapEntry(
    const FilePath::StringType& device_location,
    const base::Closure& no_references_callback)
    : device_location_(device_location),
      no_references_callback_(no_references_callback) {
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&fileapi::MTPDeviceMapService::AddDelegate,
           base::Unretained(fileapi::MTPDeviceMapService::GetInstance()),
           device_location_,
           CreateMTPDeviceDelegate(device_location_)));
#endif
}

ScopedMTPDeviceMapEntry::~ScopedMTPDeviceMapEntry() {
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&fileapi::MTPDeviceMapService::RemoveDelegate,
           base::Unretained(fileapi::MTPDeviceMapService::GetInstance()),
           device_location_));
  no_references_callback_.Run();
#endif
}

}  // namespace chrome
