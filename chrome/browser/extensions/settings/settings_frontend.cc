// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/settings_frontend.h"

#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/json/json_reader.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/settings/leveldb_settings_storage_factory.h"
#include "chrome/browser/extensions/settings/settings_backend.h"
#include "chrome/browser/extensions/settings/sync_or_local_value_store_cache.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/storage.h"
#include "content/public/browser/browser_thread.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/extensions/settings/managed_value_store_cache.h"
#endif

using content::BrowserThread;

namespace extensions {

namespace {

// Settings change Observer which forwards changes on to the extension
// processes for |profile| and its incognito partner if it exists.
class DefaultObserver : public SettingsObserver {
 public:
  explicit DefaultObserver(Profile* profile) : profile_(profile) {}

  // SettingsObserver implementation.
  virtual void OnSettingsChanged(
      const std::string& extension_id,
      settings_namespace::Namespace settings_namespace,
      const std::string& change_json) OVERRIDE {
    // TODO(gdk): This is a temporary hack while the refactoring for
    // string-based event payloads is removed. http://crbug.com/136045
    scoped_ptr<ListValue> args(new ListValue());
    args->Append(base::JSONReader::Read(change_json));
    args->Append(Value::CreateStringValue(settings_namespace::ToString(
        settings_namespace)));
    scoped_ptr<Event> event(new Event(
        event_names::kOnSettingsChanged, args.Pass()));
    ExtensionSystem::Get(profile_)->event_router()->
        DispatchEventToExtension(extension_id, event.Pass());
  }

 private:
  Profile* const profile_;
};

SettingsStorageQuotaEnforcer::Limits GetLocalLimits() {
  SettingsStorageQuotaEnforcer::Limits limits = {
    static_cast<size_t>(api::storage::local::QUOTA_BYTES),
    std::numeric_limits<size_t>::max(),
    std::numeric_limits<size_t>::max()
  };
  return limits;
}

SettingsStorageQuotaEnforcer::Limits GetSyncLimits() {
  SettingsStorageQuotaEnforcer::Limits limits = {
    static_cast<size_t>(api::storage::sync::QUOTA_BYTES),
    static_cast<size_t>(api::storage::sync::QUOTA_BYTES_PER_ITEM),
    static_cast<size_t>(api::storage::sync::MAX_ITEMS)
  };
  return limits;
}

}  // namespace

// static
SettingsFrontend* SettingsFrontend::Create(Profile* profile) {
  return new SettingsFrontend(new LeveldbSettingsStorageFactory(), profile);
}

// static
SettingsFrontend* SettingsFrontend::Create(
    const scoped_refptr<SettingsStorageFactory>& storage_factory,
    Profile* profile) {
  return new SettingsFrontend(storage_factory, profile);
}

SettingsFrontend::SettingsFrontend(
    const scoped_refptr<SettingsStorageFactory>& factory, Profile* profile)
    : local_quota_limit_(GetLocalLimits()),
      sync_quota_limit_(GetSyncLimits()),
      profile_(profile),
      observers_(new SettingsObserverList()),
      profile_observer_(new DefaultObserver(profile)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!profile->IsOffTheRecord());

  observers_->AddObserver(profile_observer_.get());

  const FilePath& profile_path = profile->GetPath();
  caches_[settings_namespace::LOCAL] =
      new SyncOrLocalValueStoreCache(
          settings_namespace::LOCAL,
          factory,
          local_quota_limit_,
          observers_,
          profile_path);
  caches_[settings_namespace::SYNC] =
      new SyncOrLocalValueStoreCache(
          settings_namespace::SYNC,
          factory,
          sync_quota_limit_,
          observers_,
          profile_path);

#if defined(ENABLE_CONFIGURATION_POLICY)
  caches_[settings_namespace::MANAGED] =
      new ManagedValueStoreCache(
          profile->GetPolicyService(),
          ExtensionSystem::Get(profile)->event_router(),
          factory,
          observers_,
          profile_path);
#endif
}

SettingsFrontend::~SettingsFrontend() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_->RemoveObserver(profile_observer_.get());
  for (CacheMap::iterator it = caches_.begin(); it != caches_.end(); ++it) {
    ValueStoreCache* cache = it->second;
    cache->ShutdownOnUI();
    BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, cache);
  }
}

syncer::SyncableService* SettingsFrontend::GetBackendForSync(
    syncer::ModelType type) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  CacheMap::const_iterator it = caches_.find(settings_namespace::SYNC);
  DCHECK(it != caches_.end());
  const SyncOrLocalValueStoreCache* sync_cache =
      static_cast<const SyncOrLocalValueStoreCache*>(it->second);
  switch (type) {
    case syncer::APP_SETTINGS:
      return sync_cache->GetAppBackend();
    case syncer::EXTENSION_SETTINGS:
      return sync_cache->GetExtensionBackend();
    default:
      NOTREACHED();
      return NULL;
  }
}

bool SettingsFrontend::IsStorageEnabled(
    settings_namespace::Namespace settings_namespace) const {
  return caches_.find(settings_namespace) != caches_.end();
}

void SettingsFrontend::RunWithStorage(
    const std::string& extension_id,
    settings_namespace::Namespace settings_namespace,
    const ValueStoreCache::StorageCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ValueStoreCache* cache = caches_[settings_namespace];
  CHECK(cache);

  // The |extension| has already been referenced earlier in the stack, so it
  // can't be gone here.
  // TODO(kalman): change RunWithStorage() to take a
  // scoped_refptr<const Extension> instead.
  scoped_refptr<const Extension> extension =
      extensions::ExtensionSystem::Get(profile_)->extension_service()->
          GetExtensionById(extension_id, true);
  CHECK(extension);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ValueStoreCache::RunWithValueStoreForExtension,
                 base::Unretained(cache), callback, extension));
}

void SettingsFrontend::DeleteStorageSoon(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (CacheMap::iterator it = caches_.begin(); it != caches_.end(); ++it) {
    ValueStoreCache* cache = it->second;
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&ValueStoreCache::DeleteStorageSoon,
                   base::Unretained(cache),
                   extension_id));
  }
}

scoped_refptr<SettingsObserverList> SettingsFrontend::GetObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return observers_;
}

void SettingsFrontend::DisableStorageForTesting(
    settings_namespace::Namespace settings_namespace) {
  CacheMap::iterator it = caches_.find(settings_namespace);
  if (it != caches_.end()) {
    ValueStoreCache* cache = it->second;
    cache->ShutdownOnUI();
    BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, cache);
    caches_.erase(it);
  }
}

}  // namespace extensions
