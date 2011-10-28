// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_frontend.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_settings_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

class ExtensionSettingsFrontend::DefaultObserver
    : public ExtensionSettingsObserver {
 public:
  explicit DefaultObserver(Profile* profile) : target_profile_(profile) {}
  virtual ~DefaultObserver() {}

  virtual void OnSettingsChanged(
      const Profile* origin_profile,
      const std::string& extension_id,
      const ExtensionSettingChanges& changes) OVERRIDE {
    if (origin_profile != target_profile_) {
      target_profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id,
        extension_event_names::kOnSettingsChanged,
        // This is the list of function arguments to pass to the onChanged
        // handler of extensions, a single argument with the list of changes.
        std::string("[") + changes.ToJson() + "]",
        target_profile_,
        GURL());
    }
  }

 private:
  Profile* target_profile_;
};

class ExtensionSettingsFrontend::Core
    : public base::RefCountedThreadSafe<Core> {
 public:
  explicit Core(
      const scoped_refptr<ObserverListThreadSafe<ExtensionSettingsObserver> >&
          observers)
      : observers_(observers), backend_(NULL) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  // Does any FILE thread specific initialization, such as construction of
  // |backend_|.  Must be called before any call to
  // RunWithBackendOnFileThread().
  void InitOnFileThread(const FilePath& base_path) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    DCHECK(!backend_);
    backend_ = new ExtensionSettingsBackend(base_path, observers_);
  }

  // Runs |callback| with the extension backend.
  void RunWithBackendOnFileThread(const BackendCallback& callback) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    DCHECK(backend_);
    callback.Run(backend_);
  }

 private:
  virtual ~Core() {
    if (BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
      delete backend_;
    } else if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, backend_);
    } else {
      NOTREACHED();
    }
  }

  friend class base::RefCountedThreadSafe<Core>;

  // Observers to settings changes (thread safe).
  scoped_refptr<ObserverListThreadSafe<ExtensionSettingsObserver> >
      observers_;

  // Lives on the FILE thread.
  ExtensionSettingsBackend* backend_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

ExtensionSettingsFrontend::ExtensionSettingsFrontend(Profile* profile)
    : profile_(profile),
      observers_(new ObserverListThreadSafe<ExtensionSettingsObserver>()),
      core_(new ExtensionSettingsFrontend::Core(observers_.get())) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!profile->IsOffTheRecord());

  // This class listens to all PROFILE_{CREATED,DESTROYED} events but we're
  // only interested in those for the original Profile given on construction
  // and its incognito version.
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
      content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::NotificationService::AllBrowserContextsAndSources());
  OnProfileCreated(profile);

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &ExtensionSettingsFrontend::Core::InitOnFileThread,
          core_.get(),
          profile->GetPath().AppendASCII(
              ExtensionService::kSettingsDirectoryName)));
}

ExtensionSettingsFrontend::~ExtensionSettingsFrontend() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ExtensionSettingsFrontend::RunWithBackend(
    const BackendCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &ExtensionSettingsFrontend::Core::RunWithBackendOnFileThread,
          core_.get(),
          callback));
}

void ExtensionSettingsFrontend::AddObserver(
    ExtensionSettingsObserver* observer) {
  observers_->AddObserver(observer);
}

void ExtensionSettingsFrontend::RemoveObserver(
    ExtensionSettingsObserver* observer) {
  observers_->RemoveObserver(observer);
}

void ExtensionSettingsFrontend::Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CREATED:
      OnProfileCreated(content::Source<Profile>(source).ptr());
      break;
    case chrome::NOTIFICATION_PROFILE_DESTROYED:
      OnProfileDestroyed(content::Source<Profile>(source).ptr());
      break;
    default:
      NOTREACHED();
  }
}

void ExtensionSettingsFrontend::OnProfileCreated(Profile* new_profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (new_profile == profile_) {
    ClearDefaultObserver(&original_profile_observer);
    SetDefaultObserver(new_profile, &original_profile_observer);
  } else if (new_profile->GetOriginalProfile() == profile_) {
    DCHECK(new_profile->IsOffTheRecord());
    ClearDefaultObserver(&incognito_profile_observer_);
    SetDefaultObserver(new_profile, &incognito_profile_observer_);
  }
}

void ExtensionSettingsFrontend::OnProfileDestroyed(Profile* old_profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (old_profile == profile_) {
    ClearDefaultObserver(&original_profile_observer);
  } else if (old_profile->GetOriginalProfile() == profile_) {
    DCHECK(old_profile->IsOffTheRecord());
    ClearDefaultObserver(&incognito_profile_observer_);
  }
}

void ExtensionSettingsFrontend::SetDefaultObserver(
    Profile* profile, scoped_ptr<DefaultObserver>* observer) {
  DCHECK(!observer->get());
  observer->reset(new DefaultObserver(profile));
  AddObserver(observer->get());
}

void ExtensionSettingsFrontend::ClearDefaultObserver(
    scoped_ptr<DefaultObserver>* observer) {
  if (observer->get()) {
    RemoveObserver(observer->get());
    observer->reset();
  }
}
