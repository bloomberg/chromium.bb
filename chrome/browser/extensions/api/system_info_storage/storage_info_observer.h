// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_STORAGE_INFO_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_STORAGE_INFO_OBSERVER_H_

#include "chrome/common/extensions/api/experimental_system_info_storage.h"

namespace extensions {

// Observes the changes happening on the storage devices, including free space
// changes, storage arrival and removal.
class StorageInfoObserver {
 public:
  virtual ~StorageInfoObserver() {}

  // Called when the storage free space changes.
  virtual void OnStorageFreeSpaceChanged(const std::string& device_id,
                                         double old_value,
                                         double new_value) {}

  // Called when a new removable storage device is attached.
  virtual void OnStorageAttached(
      const std::string& device_id,
      api::experimental_system_info_storage::StorageUnitType type,
      double capacity,
      double available_capacity) {}

  // Called when a removable storage device is detached.
  virtual void OnStorageDetached(const std::string& device_id) {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_STORAGE_INFO_OBSERVER_H_
