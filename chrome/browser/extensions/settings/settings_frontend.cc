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
#include "chrome/browser/extensions/settings/settings_leveldb_storage.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

namespace extensions {

using content::BrowserThread;

namespace {

struct Backends {
  Backends(
      // Ownership taken.
      SettingsStorageFactory* storage_factory,
      const FilePath& profile_path,
      const scoped_refptr<SettingsObserverList>& observers)
    : storage_factory_(storage_factory),
      extensions_backend_(
          storage_factory,
          profile_path.AppendASCII(
              ExtensionService::kExtensionSettingsDirectoryName),
          observers),
      apps_backend_(
          storage_factory,
          profile_path.AppendASCII(
              ExtensionService::kAppSettingsDirectoryName),
          observers) {}

  scoped_ptr<SettingsStorageFactory> storage_factory_;
  SettingsBackend extensions_backend_;
  SettingsBackend apps_backend_;
};

static void CallbackWithExtensionsBackend(
    const SettingsFrontend::SyncableServiceCallback& callback,
    Backends* backends) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  callback.Run(&backends->extensions_backend_);
}

void CallbackWithAppsBackend(
    const SettingsFrontend::SyncableServiceCallback& callback,
    Backends* backends) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  callback.Run(&backends->apps_backend_);
}

void CallbackWithExtensionsStorage(
    const std::string& extension_id,
    const SettingsFrontend::StorageCallback& callback,
    Backends* backends) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  callback.Run(backends->extensions_backend_.GetStorage(extension_id));
}

void CallbackWithAppsStorage(
    const std::string& extension_id,
    const SettingsFrontend::StorageCallback& callback,
    Backends* backends) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  callback.Run(backends->apps_backend_.GetStorage(extension_id));
}

void CallbackWithNullStorage(
    const SettingsFrontend::StorageCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  callback.Run(NULL);
}

void DeleteStorageOnFileThread(
    const std::string& extension_id, Backends* backends) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  backends->extensions_backend_.DeleteStorage(extension_id);
  backends->apps_backend_.DeleteStorage(extension_id);
}

}  // namespace

// DefaultObserver

SettingsFrontend::DefaultObserver::DefaultObserver(Profile* profile)
    : profile_(profile) {}

SettingsFrontend::DefaultObserver::~DefaultObserver() {}

void SettingsFrontend::DefaultObserver::OnSettingsChanged(
    const std::string& extension_id, const std::string& changes_json) {
  profile_->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id,
      extension_event_names::kOnSettingsChanged,
      // This is the list of function arguments to pass to the onChanged
      // handler of extensions, a single argument with the list of changes.
      std::string("[") + changes_json + "]",
      NULL,
      GURL());
}

// Core

class SettingsFrontend::Core
    : public base::RefCountedThreadSafe<Core> {
 public:
  explicit Core(
      // Ownership taken.
      SettingsStorageFactory* storage_factory,
      const scoped_refptr<SettingsObserverList>& observers)
      : storage_factory_(storage_factory),
        observers_(observers),
        backends_(NULL) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  typedef base::Callback<void(Backends*)> BackendsCallback;

  // Does any FILE thread specific initialization, such as construction of
  // |backend_|.  Must be called before any call to
  // RunWithBackendOnFileThread().
  void InitOnFileThread(const FilePath& profile_path) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    DCHECK(!backends_);
    backends_ =
        new Backends(
            storage_factory_.release(), profile_path, observers_);
  }

  // Runs |callback| with both the extensions and apps settings on the FILE
  // thread.
  void RunWithBackends(const BackendsCallback& callback) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(
            &SettingsFrontend::Core::RunWithBackendsOnFileThread,
            this,
            callback));
  }

 private:
  void RunWithBackendsOnFileThread(const BackendsCallback& callback) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    DCHECK(backends_);
    callback.Run(backends_);
  }

  virtual ~Core() {
    if (BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
      delete backends_;
    } else if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, backends_);
    } else {
      NOTREACHED();
    }
  }

  friend class base::RefCountedThreadSafe<Core>;

  // Leveldb storage area factory.  Ownership passed to Backends on Init.
  scoped_ptr<SettingsStorageFactory> storage_factory_;

  // Observers to settings changes (thread safe).
  scoped_refptr<SettingsObserverList> observers_;

  // Backends for extensions and apps settings.  Lives on FILE thread.
  Backends* backends_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

// SettingsFrontend

/* static */
SettingsFrontend* SettingsFrontend::Create(Profile* profile) {
  return new SettingsFrontend(new SettingsLeveldbStorage::Factory(), profile);
}

/* static */
SettingsFrontend* SettingsFrontend::Create(
    SettingsStorageFactory* storage_factory, Profile* profile) {
  return new SettingsFrontend(storage_factory, profile);
}

SettingsFrontend::SettingsFrontend(
    SettingsStorageFactory* storage_factory, Profile* profile)
    : profile_(profile),
      observers_(new SettingsObserverList()),
      default_observer_(profile),
      core_(new SettingsFrontend::Core(storage_factory, observers_)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!profile->IsOffTheRecord());

  observers_->AddObserver(&default_observer_);

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &SettingsFrontend::Core::InitOnFileThread,
          core_.get(),
          profile->GetPath()));
}

SettingsFrontend::~SettingsFrontend() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_->RemoveObserver(&default_observer_);
}

void SettingsFrontend::RunWithSyncableService(
    syncable::ModelType model_type, const SyncableServiceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  switch (model_type) {
    case syncable::EXTENSION_SETTINGS:
      core_->RunWithBackends(
          base::Bind(&CallbackWithExtensionsBackend, callback));
      break;
    case syncable::APP_SETTINGS:
      core_->RunWithBackends(
          base::Bind(&CallbackWithAppsBackend, callback));
      break;
    default:
      NOTREACHED();
  }
}

void SettingsFrontend::RunWithStorage(
    const std::string& extension_id,
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

  if (extension->is_app()) {
    core_->RunWithBackends(
        base::Bind(&CallbackWithAppsStorage, extension_id, callback));
  } else {
    core_->RunWithBackends(
        base::Bind(&CallbackWithExtensionsStorage, extension_id, callback));
  }
}

void SettingsFrontend::DeleteStorageSoon(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  core_->RunWithBackends(base::Bind(&DeleteStorageOnFileThread, extension_id));
}

scoped_refptr<SettingsObserverList>
SettingsFrontend::GetObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return observers_;
}

}  // namespace extensions
