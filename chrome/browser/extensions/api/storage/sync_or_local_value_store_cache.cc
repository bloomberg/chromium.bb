// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/sync_or_local_value_store_cache.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/extensions/api/storage/settings_backend.h"
#include "chrome/browser/extensions/api/storage/settings_frontend.h"
#include "chrome/browser/extensions/api/storage/settings_storage_quota_enforcer.h"
#include "chrome/browser/extensions/api/storage/weak_unlimited_settings_storage.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

SyncOrLocalValueStoreCache::SyncOrLocalValueStoreCache(
    settings_namespace::Namespace settings_namespace,
    const scoped_refptr<SettingsStorageFactory>& factory,
    const SettingsStorageQuotaEnforcer::Limits& quota,
    const scoped_refptr<SettingsObserverList>& observers,
    const FilePath& profile_path)
    : settings_namespace_(settings_namespace) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(settings_namespace_ == settings_namespace::LOCAL ||
         settings_namespace_ == settings_namespace::SYNC);

  // This post is safe since the destructor can only be invoked from the
  // same message loop, and any potential post of a deletion task must come
  // after the constructor returns.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SyncOrLocalValueStoreCache::InitOnFileThread,
                 base::Unretained(this),
                 factory, quota, observers, profile_path));
}

SyncOrLocalValueStoreCache::~SyncOrLocalValueStoreCache() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

SettingsBackend* SyncOrLocalValueStoreCache::GetAppBackend() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(app_backend_.get());
  return app_backend_.get();
}

SettingsBackend* SyncOrLocalValueStoreCache::GetExtensionBackend() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(extension_backend_.get());
  return extension_backend_.get();
}

void SyncOrLocalValueStoreCache::RunWithValueStoreForExtension(
    const StorageCallback& callback,
    scoped_refptr<const Extension> extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(app_backend_.get());
  DCHECK(extension_backend_.get());
  SettingsBackend* backend =
      extension->is_app() ? app_backend_.get() : extension_backend_.get();
  ValueStore* storage = backend->GetStorage(extension->id());

  // A neat way to implement unlimited storage; if the extension has the
  // unlimited storage permission, force through all calls to Set() (in the
  // same way that writes from sync ignore quota).
  // But only if it's local storage (bad stuff would happen if sync'ed
  // storage is allowed to be unlimited).
  bool is_unlimited =
      settings_namespace_ == settings_namespace::LOCAL &&
      extension->HasAPIPermission(APIPermission::kUnlimitedStorage);

  if (is_unlimited) {
    WeakUnlimitedSettingsStorage unlimited_storage(storage);
    callback.Run(&unlimited_storage);
  } else {
    callback.Run(storage);
  }
}

void SyncOrLocalValueStoreCache::DeleteStorageSoon(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  app_backend_->DeleteStorage(extension_id);
  extension_backend_->DeleteStorage(extension_id);
}

void SyncOrLocalValueStoreCache::InitOnFileThread(
    const scoped_refptr<SettingsStorageFactory>& factory,
    const SettingsStorageQuotaEnforcer::Limits& quota,
    const scoped_refptr<SettingsObserverList>& observers,
    const FilePath& profile_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!app_backend_.get());
  DCHECK(!extension_backend_.get());
  const bool local = settings_namespace_ == settings_namespace::LOCAL;
  const FilePath app_path = profile_path.AppendASCII(
      local ? ExtensionService::kLocalAppSettingsDirectoryName
            : ExtensionService::kSyncAppSettingsDirectoryName);
  const FilePath extension_path = profile_path.AppendASCII(
      local ? ExtensionService::kLocalExtensionSettingsDirectoryName
            : ExtensionService::kSyncExtensionSettingsDirectoryName);
  app_backend_.reset(new SettingsBackend(factory, app_path, quota, observers));
  extension_backend_.reset(
      new SettingsBackend(factory, extension_path, quota, observers));
}

}  // namespace extensions
