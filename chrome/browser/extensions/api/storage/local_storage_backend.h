// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_LOCAL_STORAGE_BACKEND_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_LOCAL_STORAGE_BACKEND_H_

#include <map>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/storage/settings_backend.h"
#include "chrome/browser/extensions/api/storage/settings_storage_quota_enforcer.h"

namespace base {
class FilePath;
}

class ValueStore;

namespace extensions {
class SettingStorageFactory;

class LocalStorageBackend : public SettingsBackend {
 public:
  LocalStorageBackend(
      const scoped_refptr<SettingsStorageFactory>& storage_factory,
      const base::FilePath& base_path,
      const SettingsStorageQuotaEnforcer::Limits& quota);
  virtual ~LocalStorageBackend();

  // SettingsBackend implementation.
  virtual ValueStore* GetStorage(const std::string& extension_id) OVERRIDE;
  virtual void DeleteStorage(const std::string& extension_id) OVERRIDE;

 private:
  typedef std::map<std::string, linked_ptr<ValueStore> > StorageMap;

  StorageMap storage_map_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageBackend);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_LOCAL_STORAGE_BACKEND_H_
