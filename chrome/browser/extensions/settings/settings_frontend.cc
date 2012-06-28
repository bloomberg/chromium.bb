// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/settings_frontend.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings/leveldb_settings_storage_factory.h"
#include "chrome/browser/extensions/settings/settings_backend.h"
#include "chrome/browser/extensions/settings/settings_namespace.h"
#include "chrome/browser/extensions/settings/weak_unlimited_settings_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/value_store/leveldb_value_store.h"
#include "chrome/common/extensions/api/storage.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

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
    profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id,
        extension_event_names::kOnSettingsChanged,
        // This is the list of function arguments to pass to the onChanged
        // handler of extensions, an array of [changes, settings_namespace].
        std::string("[") + change_json + ",\"" +
            settings_namespace::ToString(settings_namespace) + "\"]",
        NULL,
        GURL());
  }

 private:
  Profile* const profile_;
};

void CallbackWithSyncableService(
    const SettingsFrontend::SyncableServiceCallback& callback,
    SettingsBackend* backend) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  callback.Run(backend);
}

void CallbackWithStorage(
    const std::string& extension_id,
    const SettingsFrontend::StorageCallback& callback,
    SettingsBackend* backend) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  callback.Run(backend->GetStorage(extension_id));
}

void CallbackWithNullStorage(
    const SettingsFrontend::StorageCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  callback.Run(NULL);
}

void DeleteStorageOnFileThread(
    const std::string& extension_id, SettingsBackend* backend) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  backend->DeleteStorage(extension_id);
}

void CallbackWithUnlimitedStorage(
    const std::string& extension_id,
    const SettingsFrontend::StorageCallback& callback,
    SettingsBackend* backend) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  WeakUnlimitedSettingsStorage unlimited_storage(
      backend->GetStorage(extension_id));
  callback.Run(&unlimited_storage);
}

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

// Ref-counted container for a SettingsBackend object.
class SettingsFrontend::BackendWrapper
    : public base::RefCountedThreadSafe<BackendWrapper> {
 public:
  // Creates a new BackendWrapper and initializes it on the FILE thread.
  static scoped_refptr<BackendWrapper> CreateAndInit(
      const scoped_refptr<SettingsStorageFactory>& factory,
      const SettingsStorageQuotaEnforcer::Limits& quota,
      const scoped_refptr<SettingsObserverList>& observers,
      const FilePath& path) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    scoped_refptr<BackendWrapper> backend_wrapper =
        new BackendWrapper(factory, quota, observers);
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(
            &SettingsFrontend::BackendWrapper::InitOnFileThread,
            backend_wrapper,
            path));
    return backend_wrapper;
  }

  typedef base::Callback<void(SettingsBackend*)> BackendCallback;

  // Runs |callback| with the wrapped Backend on the FILE thread.
  void RunWithBackend(const BackendCallback& callback) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(
            &SettingsFrontend::BackendWrapper::RunWithBackendOnFileThread,
            this,
            callback));
  }

  SettingsBackend* GetBackend() const {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    return backend_;
  }

 private:
  friend class base::RefCountedThreadSafe<BackendWrapper>;

  BackendWrapper(
      const scoped_refptr<SettingsStorageFactory>& storage_factory,
      const SettingsStorageQuotaEnforcer::Limits& quota,
      const scoped_refptr<SettingsObserverList>& observers)
      : storage_factory_(storage_factory),
        quota_(quota),
        observers_(observers),
        backend_(NULL) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  virtual ~BackendWrapper() {
    if (BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
      delete backend_;
    } else if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, backend_);
    } else {
      NOTREACHED();
    }
  }

  void InitOnFileThread(const FilePath& path) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    DCHECK(!backend_);
    backend_ = new SettingsBackend(storage_factory_, path, quota_, observers_);
    storage_factory_ = NULL;
    observers_ = NULL;
  }

  void RunWithBackendOnFileThread(const BackendCallback& callback) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    DCHECK(backend_);
    callback.Run(backend_);
  }

  // Only need these until |backend_| exists.
  scoped_refptr<SettingsStorageFactory> storage_factory_;
  const SettingsStorageQuotaEnforcer::Limits quota_;
  scoped_refptr<SettingsObserverList> observers_;

  // Wrapped Backend.  Used exclusively on the FILE thread, and is created on
  // the FILE thread in InitOnFileThread.
  SettingsBackend* backend_;

  DISALLOW_COPY_AND_ASSIGN(BackendWrapper);
};

// SettingsFrontend

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
  backends_[settings_namespace::LOCAL].app =
      BackendWrapper::CreateAndInit(
          factory,
          local_quota_limit_,
          observers_,
          profile_path.AppendASCII(
              ExtensionService::kLocalAppSettingsDirectoryName));
  backends_[settings_namespace::LOCAL].extension =
      BackendWrapper::CreateAndInit(
          factory,
          local_quota_limit_,
          observers_,
          profile_path.AppendASCII(
              ExtensionService::kLocalExtensionSettingsDirectoryName));
  backends_[settings_namespace::SYNC].app =
      BackendWrapper::CreateAndInit(
          factory,
          sync_quota_limit_,
          observers_,
          profile_path.AppendASCII(
              ExtensionService::kSyncAppSettingsDirectoryName));
  backends_[settings_namespace::SYNC].extension =
      BackendWrapper::CreateAndInit(
          factory,
          sync_quota_limit_,
          observers_,
          profile_path.AppendASCII(
              ExtensionService::kSyncExtensionSettingsDirectoryName));
}

SettingsFrontend::~SettingsFrontend() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_->RemoveObserver(profile_observer_.get());
}

syncer::SyncableService* SettingsFrontend::GetBackendForSync(
    syncable::ModelType type) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::map<settings_namespace::Namespace, BackendWrappers>::const_iterator
      sync_backends = backends_.find(settings_namespace::SYNC);
  DCHECK(sync_backends != backends_.end());
  switch (type) {
    case syncable::APP_SETTINGS:
      return sync_backends->second.app->GetBackend();
    case syncable::EXTENSION_SETTINGS:
      return sync_backends->second.extension->GetBackend();
    default:
      NOTREACHED();
      return NULL;
  }
}

void SettingsFrontend::RunWithStorage(
    const std::string& extension_id,
    settings_namespace::Namespace settings_namespace,
    const StorageCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const Extension* extension =
      profile_->GetExtensionService()->GetExtensionById(extension_id, true);
  if (!extension) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&CallbackWithNullStorage, callback));
    return;
  }

  // A neat way to implement unlimited storage; if the extension has the
  // unlimited storage permission, force through all calls to Set() (in the
  // same way that writes from sync ignore quota).
  // But only if it's local storage (bad stuff would happen if sync'ed
  // storage is allowed to be unlimited).
  bool is_unlimited =
      settings_namespace == settings_namespace::LOCAL &&
      extension->HasAPIPermission(APIPermission::kUnlimitedStorage);

  scoped_refptr<BackendWrapper> backend;
  if (extension->is_app()) {
    backend = backends_[settings_namespace].app;
  } else {
    backend = backends_[settings_namespace].extension;
  }

  backend->RunWithBackend(
      base::Bind(
          is_unlimited ?
              &CallbackWithUnlimitedStorage : &CallbackWithStorage,
          extension_id,
          callback));
}

void SettingsFrontend::DeleteStorageSoon(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SettingsFrontend::BackendWrapper::BackendCallback callback =
      base::Bind(&DeleteStorageOnFileThread, extension_id);
  for (std::map<settings_namespace::Namespace, BackendWrappers>::iterator it =
      backends_.begin(); it != backends_.end(); ++it) {
    it->second.app->RunWithBackend(callback);
    it->second.extension->RunWithBackend(callback);
  }
}

scoped_refptr<SettingsObserverList> SettingsFrontend::GetObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return observers_;
}

// BackendWrappers

SettingsFrontend::BackendWrappers::BackendWrappers() {}
SettingsFrontend::BackendWrappers::~BackendWrappers() {}

}  // namespace extensions
