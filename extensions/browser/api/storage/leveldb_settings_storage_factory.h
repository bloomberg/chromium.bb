// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_LEVELDB_SETTINGS_STORAGE_FACTORY_H_
#define EXTENSIONS_BROWSER_API_STORAGE_LEVELDB_SETTINGS_STORAGE_FACTORY_H_

#include "extensions/browser/api/storage/settings_storage_factory.h"

namespace extensions {

// Factory for creating LeveldbValueStore instances.
class LeveldbSettingsStorageFactory : public SettingsStorageFactory {
 public:
  virtual ValueStore* Create(const base::FilePath& base_path,
                             const std::string& extension_id) OVERRIDE;

  virtual void DeleteDatabaseIfExists(const base::FilePath& base_path,
                                      const std::string& extension_id) OVERRIDE;

 private:
  // SettingsStorageFactory is refcounted.
  virtual ~LeveldbSettingsStorageFactory() {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_LEVELDB_SETTINGS_STORAGE_FACTORY_H_
