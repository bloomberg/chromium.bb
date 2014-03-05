// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_BACKEND_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_BACKEND_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/storage/settings_storage_quota_enforcer.h"

namespace syncer {
class SyncableService;
}

class ValueStore;

namespace extensions {
class SettingsStorageFactory;

class SettingsBackend {
 public:
  SettingsBackend(const scoped_refptr<SettingsStorageFactory>& storage_factory,
                  const base::FilePath& base_path,
                  const SettingsStorageQuotaEnforcer::Limits& quota);
  virtual ~SettingsBackend();

  // Gets a weak reference to the storage area for |extension_id|.
  // Must be run on the FILE thread.
  virtual ValueStore* GetStorage(const std::string& extension_id) = 0;

  // Deletes all setting data for an extension. Call on the FILE thread.
  virtual void DeleteStorage(const std::string& extension_id) = 0;

  // A slight hack so we can get a SyncableService from a SettingsBackend if
  // it's actually a SyncStorageBackend. If called on a LocalStorageBackend,
  // this asserts and returns null.
  virtual syncer::SyncableService* GetAsSyncableService();

  SettingsStorageFactory* storage_factory() const {
    return storage_factory_.get();
  }
  const base::FilePath& base_path() const { return base_path_; }
  const SettingsStorageQuotaEnforcer::Limits& quota() const { return quota_; }

 protected:
  // Creates a ValueStore decorated with quota-enforcing behavior (the default
  // for both sync and local stores). If the database is corrupt,
  // SettingsStorageQuotaEnforcer will try and restore it as part of the
  // initialization process (by necessity, since we need to read the database to
  // calculate the storage).
  scoped_ptr<SettingsStorageQuotaEnforcer> CreateStorageForExtension(
      const std::string& extension_id) const;

 private:
  // The Factory to use for creating new ValueStores.
  const scoped_refptr<SettingsStorageFactory> storage_factory_;

  // The base file path to use when creating new ValueStores.
  const base::FilePath base_path_;

  // Quota limits (see SettingsStorageQuotaEnforcer).
  const SettingsStorageQuotaEnforcer::Limits quota_;

  DISALLOW_COPY_AND_ASSIGN(SettingsBackend);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTINGS_BACKEND_H_
