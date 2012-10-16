// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/magnification_manager.h"

#include "ash/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/api/prefs/pref_change_registrar.h"
#include "chrome/browser/api/prefs/pref_member.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

class MagnificationManagerImpl : public MagnificationManager,
                                 public content::NotificationObserver {
 public:
  MagnificationManagerImpl() {
    DCHECK(!instance_);
    instance_ = this;

    registrar_.Add(this,
                  chrome::NOTIFICATION_SESSION_STARTED,
                  content::NotificationService::AllSources());
    registrar_.Add(this,
                  chrome::NOTIFICATION_PROFILE_CREATED,
                  content::NotificationService::AllSources());
    registrar_.Add(this,
                  chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE,
                  content::NotificationService::AllSources());

    Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    SetProfile(profile);
  }
  virtual ~MagnificationManagerImpl() {}

  static MagnificationManagerImpl* GetInstance() {
    return instance_;
  }

  // MagnificationManager implimentation:
  bool IsEnabled() OVERRIDE {
    return ash::Shell::GetInstance()->magnification_controller()->IsEnabled();
  }

  void SetEnabled(bool enabled) OVERRIDE {
    ash::Shell::GetInstance()->magnification_controller()->SetEnabled(enabled);
  }

 private:
  void SetProfile(Profile* profile) {
    if (pref_change_registrar_) {
      pref_change_registrar_->Remove(prefs::kScreenMagnifierEnabled, this);
      pref_change_registrar_.reset();
    }

    if (profile) {
      pref_change_registrar_.reset(new PrefChangeRegistrar);
      pref_change_registrar_->Init(profile->GetPrefs());
      pref_change_registrar_->Add(prefs::kScreenMagnifierEnabled, this);
    }

    profile_ = profile;
    UpdateMagnifierStatus();
  }

  void UpdateMagnifierStatus() {
    UserManager* manager = UserManager::Get();
    if (!profile_) {
      SetEnabled(false);
    } else if (manager && !manager->IsSessionStarted()) {
      SetEnabled(true);
    } else {
      PrefService* pref_service = profile_->GetPrefs();
      bool enabled = pref_service->GetBoolean(prefs::kScreenMagnifierEnabled);
      SetEnabled(enabled);
    }
  }

  // content::NotificationObserver implimentation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_PREF_CHANGED: {
        std::string pref = *content::Details<std::string>(details).ptr();
        if (pref == prefs::kScreenMagnifierEnabled)
          UpdateMagnifierStatus();
        break;
      }
      case chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE:
      case chrome::NOTIFICATION_SESSION_STARTED: {
        Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
        SetProfile(profile);
        break;
      }
    }
  }

  static MagnificationManagerImpl* instance_;

  Profile* profile_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<PrefChangeRegistrar> pref_change_registrar_;

  friend struct DefaultSingletonTraits<MagnificationManagerImpl>;
  DISALLOW_COPY_AND_ASSIGN(MagnificationManagerImpl);
};

MagnificationManagerImpl* MagnificationManagerImpl::instance_ = NULL;

MagnificationManager* MagnificationManager::GetInstance() {
  return MagnificationManagerImpl::GetInstance();
}

MagnificationManager* MagnificationManager::CreateInstance() {
  // Makes sure that this is not called more than once.
  CHECK(!GetInstance());

  return new MagnificationManagerImpl();
}

}  // namespace chromeos
