// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/local_storage_backend.h"

#include "base/file_util.h"
#include "extensions/browser/api/storage/settings_storage_factory.h"

namespace extensions {

LocalStorageBackend::LocalStorageBackend(
    const scoped_refptr<SettingsStorageFactory>& storage_factory,
    const base::FilePath& base_path,
    const SettingsStorageQuotaEnforcer::Limits& quota)
    : SettingsBackend(storage_factory, base_path, quota) {}

LocalStorageBackend::~LocalStorageBackend() {}

ValueStore* LocalStorageBackend::GetStorage(const std::string& extension_id) {
  StorageMap::iterator iter = storage_map_.find(extension_id);
  if (iter != storage_map_.end())
    return iter->second.get();

  linked_ptr<SettingsStorageQuotaEnforcer> storage(
      CreateStorageForExtension(extension_id).release());
  storage_map_[extension_id] = storage;
  return storage.get();
}

void LocalStorageBackend::DeleteStorage(const std::string& extension_id) {
  // Clear settings when the extension is uninstalled.
  storage_map_.erase(extension_id);
  storage_factory()->DeleteDatabaseIfExists(base_path(), extension_id);
}

}  // namespace extensions
