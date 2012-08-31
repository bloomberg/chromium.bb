// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"

namespace extensions {

namespace {

using api::experimental_system_info_storage::StorageUnitInfo;

// StorageInfoProvider implementation on Macdows platform.
// TODO(hongbo): not implemented yet.
class StorageInfoProviderMac : public StorageInfoProvider {
 public:
  StorageInfoProviderMac() {}
  virtual ~StorageInfoProviderMac() {}

  virtual bool QueryInfo(StorageInfo* info) OVERRIDE;
  virtual bool QueryUnitInfo(const std::string& id,
                             StorageUnitInfo* info) OVERRIDE;
};

bool StorageInfoProviderMac::QueryInfo(StorageInfo* info) {
  return false;
}

bool StorageInfoProviderMac::QueryUnitInfo(const std::string& id,
                                           StorageUnitInfo* info) {
  return false;
}

}  // namespace

// static
StorageInfoProvider* StorageInfoProvider::Get() {
  return StorageInfoProvider::GetInstance<StorageInfoProviderMac>();
}

}  // namespace extensions
