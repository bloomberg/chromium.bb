// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"

namespace extensions {

bool RemovableStorageProvider::PopulateDeviceList(
    scoped_refptr<StorageDeviceList> device_list) {
  return false;
}

} // namespace extensions
