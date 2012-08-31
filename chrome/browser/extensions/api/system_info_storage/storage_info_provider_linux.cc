// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"

namespace extensions {

namespace {

using api::experimental_system_info_storage::StorageUnitInfo;

// StorageInfoProvider implementation on Linux platform.
// TODO(hongbo): not implemented yet.
class StorageInfoProviderLinux : public StorageInfoProvider {
 public:
  StorageInfoProviderLinux() {}
  virtual ~StorageInfoProviderLinux() {}

  virtual bool QueryInfo(StorageInfo* info) OVERRIDE;
  virtual bool QueryUnitInfo(const std::string& id,
                             StorageUnitInfo* info) OVERRIDE;
};

bool StorageInfoProviderLinux::QueryInfo(StorageInfo* info) {
  return false;
}

bool StorageInfoProviderLinux::QueryUnitInfo(const std::string& id,
                                             StorageUnitInfo* info) {
  return false;
}

}   // namespace

// static
StorageInfoProvider* StorageInfoProvider::Get() {
  return StorageInfoProvider::GetInstance<StorageInfoProviderLinux>();
}

}  // namespace extensions
