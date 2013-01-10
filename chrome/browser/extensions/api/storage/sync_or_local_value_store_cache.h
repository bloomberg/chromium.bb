// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_OR_LOCAL_VALUE_STORE_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_OR_LOCAL_VALUE_STORE_CACHE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/storage/settings_observer.h"
#include "chrome/browser/extensions/api/storage/settings_storage_quota_enforcer.h"
#include "chrome/browser/extensions/api/storage/value_store_cache.h"

class FilePath;

namespace extensions {

class SettingsBackend;
class SettingsStorageFactory;

// ValueStoreCache for the LOCAL and SYNC namespaces. It owns a backend for
// apps and another for extensions. Each backend takes care of persistence and
// syncing.
class SyncOrLocalValueStoreCache : public ValueStoreCache {
 public:
  SyncOrLocalValueStoreCache(
      settings_namespace::Namespace settings_namespace,
      const scoped_refptr<SettingsStorageFactory>& factory,
      const SettingsStorageQuotaEnforcer::Limits& quota,
      const scoped_refptr<SettingsObserverList>& observers,
      const FilePath& profile_path);
  virtual ~SyncOrLocalValueStoreCache();

  SettingsBackend* GetAppBackend() const;
  SettingsBackend* GetExtensionBackend() const;

  // ValueStoreCache implementation:

  virtual void RunWithValueStoreForExtension(
      const StorageCallback& callback,
      scoped_refptr<const Extension> extension) OVERRIDE;

  virtual void DeleteStorageSoon(const std::string& extension_id) OVERRIDE;

 private:
  void InitOnFileThread(const scoped_refptr<SettingsStorageFactory>& factory,
                        const SettingsStorageQuotaEnforcer::Limits& quota,
                        const scoped_refptr<SettingsObserverList>& observers,
                        const FilePath& profile_path);

  settings_namespace::Namespace settings_namespace_;
  scoped_ptr<SettingsBackend> app_backend_;
  scoped_ptr<SettingsBackend> extension_backend_;

  DISALLOW_COPY_AND_ASSIGN(SyncOrLocalValueStoreCache);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_OR_LOCAL_VALUE_STORE_CACHE_H_
