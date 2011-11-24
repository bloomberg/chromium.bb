// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/settings_frontend.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings/settings_backend.h"
#include "chrome/browser/extensions/settings/settings_namespace.h"
#include "chrome/browser/extensions/settings/settings_leveldb_storage.h"
#include "chrome/browser/profiles/profile.h"
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

}  // namespace

// Ref-counted container for a SettingsBackend object.
class SettingsFrontend::BackendWrapper
    : public base::RefCountedThreadSafe<BackendWrapper> {
 public:
  // Creates a new BackendWrapper and initializes it on the FILE thread.
  static scoped_refptr<BackendWrapper> CreateAndInit(
      const scoped_refptr<SettingsStorageFactory>& factory,
      const scoped_refptr<SettingsObserverList>& observers,
      const FilePath& path) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    scoped_refptr<BackendWrapper> backend_wrapper =
        new BackendWrapper(factory, observers);
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

 private:
  friend class base::RefCountedThreadSafe<BackendWrapper>;

  BackendWrapper(
      const scoped_refptr<SettingsStorageFactory>& storage_factory,
      const scoped_refptr<SettingsObserverList>& observers)
      : storage_factory_(storage_factory),
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
    backend_ = new SettingsBackend(storage_factory_, path, observers_);
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
  scoped_refptr<SettingsObserverList> observers_;

  // Wrapped Backend.  Used exclusively on the FILE thread, and is created on
  // the FILE thread in InitOnFileThread.
  SettingsBackend* backend_;

  DISALLOW_COPY_AND_ASSIGN(BackendWrapper);
};

// SettingsFrontend

/* static */
SettingsFrontend* SettingsFrontend::Create(Profile* profile) {
  return new SettingsFrontend(new SettingsLeveldbStorage::Factory(), profile);
}

/* static */
SettingsFrontend* SettingsFrontend::Create(
    const scoped_refptr<SettingsStorageFactory>& storage_factory,
    Profile* profile) {
  return new SettingsFrontend(storage_factory, profile);
}

SettingsFrontend::SettingsFrontend(
    const scoped_refptr<SettingsStorageFactory>& factory, Profile* profile)
    : profile_(profile),
      observers_(new SettingsObserverList()),
      profile_observer_(new DefaultObserver(profile)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!profile->IsOffTheRecord());

  observers_->AddObserver(profile_observer_.get());

  const FilePath& profile_path = profile->GetPath();
  backends_[settings_namespace::LOCAL].app =
      BackendWrapper::CreateAndInit(
          factory,
          observers_,
          profile_path.AppendASCII(
              ExtensionService::kLocalAppSettingsDirectoryName));
  backends_[settings_namespace::LOCAL].extension =
      BackendWrapper::CreateAndInit(
          factory,
          observers_,
          profile_path.AppendASCII(
              ExtensionService::kLocalExtensionSettingsDirectoryName));
  backends_[settings_namespace::SYNC].app =
      BackendWrapper::CreateAndInit(
          factory,
          observers_,
          profile_path.AppendASCII(
              ExtensionService::kSyncAppSettingsDirectoryName));
  backends_[settings_namespace::SYNC].extension =
      BackendWrapper::CreateAndInit(
          factory,
          observers_,
          profile_path.AppendASCII(
              ExtensionService::kSyncExtensionSettingsDirectoryName));
}

SettingsFrontend::~SettingsFrontend() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_->RemoveObserver(profile_observer_.get());
}

void SettingsFrontend::RunWithSyncableService(
    syncable::ModelType model_type, const SyncableServiceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_refptr<BackendWrapper> backend;
  switch (model_type) {
    case syncable::APP_SETTINGS:
      backend = backends_[settings_namespace::SYNC].app;
      break;

    case syncable::EXTENSION_SETTINGS:
      backend = backends_[settings_namespace::SYNC].extension;
      break;

    default:
      NOTREACHED();
  }
  backend->RunWithBackend(base::Bind(&CallbackWithSyncableService, callback));
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

  scoped_refptr<BackendWrapper> backend;
  if (extension->is_app()) {
    backend = backends_[settings_namespace].app;
  } else {
    backend = backends_[settings_namespace].extension;
  }
  backend->RunWithBackend(
      base::Bind(&CallbackWithStorage, extension_id, callback));
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
