// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/recovery_private/removable_storage_provider.h"

namespace extensions {

void RemovableStorageProvider::GetAllDevices(
    DeviceListReadyCallback callback) {
  scoped_refptr<StorageDeviceList> device_list(new StorageDeviceList());

  callback.Run(device_list, false);
}

} // namespace extensions
