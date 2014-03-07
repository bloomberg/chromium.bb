// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/local_value_store_cache.h"

#include <limits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/api/storage/local_storage_backend.h"
#include "chrome/browser/extensions/api/storage/settings_storage_factory.h"
#include "chrome/browser/extensions/api/storage/settings_storage_quota_enforcer.h"
#include "chrome/browser/extensions/api/storage/weak_unlimited_settings_storage.h"
#include "chrome/common/extensions/api/storage.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"

using content::BrowserThread;

namespace extensions {

namespace {

// Returns the quota limit for local storage, taken from the schema in
// chrome/common/extensions/api/storage.json.
SettingsStorageQuotaEnforcer::Limits GetLocalQuotaLimits() {
  SettingsStorageQuotaEnforcer::Limits limits = {
    static_cast<size_t>(api::storage::local::QUOTA_BYTES),
    std::numeric_limits<size_t>::max(),
    std::numeric_limits<size_t>::max()
  };
  return limits;
}

}  // namespace

LocalValueStoreCache::LocalValueStoreCache(
    const scoped_refptr<SettingsStorageFactory>& factory,
    const base::FilePath& profile_path)
    : initialized_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // This post is safe since the destructor can only be invoked from the
  // same message loop, and any potential post of a deletion task must come
  // after the constructor returns.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&LocalValueStoreCache::InitOnFileThread,
                 base::Unretained(this),
                 factory, profile_path));
}

LocalValueStoreCache::~LocalValueStoreCache() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

void LocalValueStoreCache::RunWithValueStoreForExtension(
    const StorageCallback& callback,
    scoped_refptr<const Extension> extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(initialized_);

  SettingsBackend* backend =
      extension->is_app() ? app_backend_.get() : extension_backend_.get();
  ValueStore* storage = backend->GetStorage(extension->id());

  // A neat way to implement unlimited storage; if the extension has the
  // unlimited storage permission, force through all calls to Set().
  if (extension->HasAPIPermission(APIPermission::kUnlimitedStorage)) {
    WeakUnlimitedSettingsStorage unlimited_storage(storage);
    callback.Run(&unlimited_storage);
  } else {
    callback.Run(storage);
  }
}

void LocalValueStoreCache::DeleteStorageSoon(const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(initialized_);
  app_backend_->DeleteStorage(extension_id);
  extension_backend_->DeleteStorage(extension_id);
}

void LocalValueStoreCache::InitOnFileThread(
    const scoped_refptr<SettingsStorageFactory>& factory,
    const base::FilePath& profile_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!initialized_);
  app_backend_.reset(new LocalStorageBackend(
      factory,
      profile_path.AppendASCII(kLocalAppSettingsDirectoryName),
      GetLocalQuotaLimits()));
  extension_backend_.reset(new LocalStorageBackend(
      factory,
      profile_path.AppendASCII(kLocalExtensionSettingsDirectoryName),
      GetLocalQuotaLimits()));
  initialized_ = true;
}

}  // namespace extensions
