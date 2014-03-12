// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/api/storage/value_store_cache.h"
#include "sync/api/syncable_service.h"

namespace base {
class FilePath;
}

namespace syncer {
class SyncableService;
}

namespace extensions {

class SettingsStorageFactory;
class SyncStorageBackend;

// ValueStoreCache for the SYNC namespace. It owns a backend for apps and
// another for extensions. Each backend takes care of persistence and syncing.
class SyncValueStoreCache : public ValueStoreCache {
 public:
  SyncValueStoreCache(
      const scoped_refptr<SettingsStorageFactory>& factory,
      const scoped_refptr<SettingsObserverList>& observers,
      const base::FilePath& profile_path);
  virtual ~SyncValueStoreCache();

  syncer::SyncableService* GetSyncableService(syncer::ModelType type) const;

  // ValueStoreCache implementation:
  virtual void RunWithValueStoreForExtension(
      const StorageCallback& callback,
      scoped_refptr<const Extension> extension) OVERRIDE;
  virtual void DeleteStorageSoon(const std::string& extension_id) OVERRIDE;

 private:
  void InitOnFileThread(const scoped_refptr<SettingsStorageFactory>& factory,
                        const scoped_refptr<SettingsObserverList>& observers,
                        const base::FilePath& profile_path);

  bool initialized_;
  scoped_ptr<SyncStorageBackend> app_backend_;
  scoped_ptr<SyncStorageBackend> extension_backend_;

  DISALLOW_COPY_AND_ASSIGN(SyncValueStoreCache);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_VALUE_STORE_CACHE_H_
