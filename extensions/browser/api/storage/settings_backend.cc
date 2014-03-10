// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/settings_backend.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/settings_storage_factory.h"

using content::BrowserThread;

namespace extensions {

SettingsBackend::SettingsBackend(
    const scoped_refptr<SettingsStorageFactory>& storage_factory,
    const base::FilePath& base_path,
    const SettingsStorageQuotaEnforcer::Limits& quota)
    : storage_factory_(storage_factory), base_path_(base_path), quota_(quota) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

SettingsBackend::~SettingsBackend() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

syncer::SyncableService* SettingsBackend::GetAsSyncableService() {
  NOTREACHED();
  return NULL;
}

scoped_ptr<SettingsStorageQuotaEnforcer>
SettingsBackend::CreateStorageForExtension(
    const std::string& extension_id) const {
  scoped_ptr<SettingsStorageQuotaEnforcer> storage(
      new SettingsStorageQuotaEnforcer(
          quota(), storage_factory()->Create(base_path(), extension_id)));
  DCHECK(storage.get());
  return storage.Pass();
}

}  // namespace extensions
