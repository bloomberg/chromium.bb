// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"

namespace extensions {

namespace {

using api::experimental_system_info_storage::StorageUnitInfo;

// Empty StorageInfoProvider implementation on Android platform.
class StorageInfoProviderAndroid : public StorageInfoProvider {
 public:
  StorageInfoProviderAndroid() {}

  virtual bool QueryInfo(StorageInfo* info) OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }
  virtual bool QueryUnitInfo(const std::string& id,
                             StorageUnitInfo* info) OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }

 private:
  virtual ~StorageInfoProviderAndroid() {}
};

}  //

// static
StorageInfoProvider* StorageInfoProvider::Get() {
  return StorageInfoProvider::GetInstance<StorageInfoProviderAndroid>();
}

}  // namespace extensions

