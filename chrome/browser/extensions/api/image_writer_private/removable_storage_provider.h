// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_REMOVABLE_STORAGE_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_REMOVABLE_STORAGE_PROVIDER_H_

#include "base/callback.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/common/extensions/api/image_writer_private.h"
#include "chrome/common/ref_counted_util.h"

namespace extensions {

typedef RefCountedVector<linked_ptr
  <api::image_writer_private::RemovableStorageDevice> > StorageDeviceList;

// Abstraction for platform specific implementations of listing removable
// storage devices
class RemovableStorageProvider {
 public:
  typedef base::Callback<void(scoped_refptr<StorageDeviceList>, bool)>
    DeviceListReadyCallback;
  static void GetAllDevices(DeviceListReadyCallback callback);
#if defined(OS_LINUX)
  static bool GetDevicesOnFileThread(scoped_refptr<StorageDeviceList>);
#endif
};

} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_REMOVABLE_STORAGE_PROVIDER_H_
