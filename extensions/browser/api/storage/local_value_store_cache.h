// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_LOCAL_VALUE_STORE_CACHE_H_
#define EXTENSIONS_BROWSER_API_STORAGE_LOCAL_VALUE_STORE_CACHE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/browser/api/storage/value_store_cache.h"

namespace base {
class FilePath;
}

namespace extensions {

class SettingsBackend;
class SettingsStorageFactory;

// ValueStoreCache for the LOCAL namespace. It owns a backend for apps and
// another for extensions. Each backend takes care of persistence.
class LocalValueStoreCache : public ValueStoreCache {
 public:
  LocalValueStoreCache(const scoped_refptr<SettingsStorageFactory>& factory,
                       const base::FilePath& profile_path);
  virtual ~LocalValueStoreCache();

  // ValueStoreCache implementation:
  virtual void RunWithValueStoreForExtension(
      const StorageCallback& callback,
      scoped_refptr<const Extension> extension) OVERRIDE;
  virtual void DeleteStorageSoon(const std::string& extension_id) OVERRIDE;

 private:
  void InitOnFileThread(const scoped_refptr<SettingsStorageFactory>& factory,
                        const base::FilePath& profile_path);

  bool initialized_;
  scoped_ptr<SettingsBackend> app_backend_;
  scoped_ptr<SettingsBackend> extension_backend_;

  DISALLOW_COPY_AND_ASSIGN(LocalValueStoreCache);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_LOCAL_VALUE_STORE_CACHE_H_
