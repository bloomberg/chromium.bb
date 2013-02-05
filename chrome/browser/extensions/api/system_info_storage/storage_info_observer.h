// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_STORAGE_INFO_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_STORAGE_INFO_OBSERVER_H_

namespace extensions {

// Observes the free space changes happening on the storage.
class StorageInfoObserver {
 public:
  virtual ~StorageInfoObserver() {}

  // Called when the storage free space changes.
  virtual void OnStorageFreeSpaceChanged(const std::string& id,
                                         double old_value,
                                         double new_value) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_STORAGE_INFO_OBSERVER_H_
