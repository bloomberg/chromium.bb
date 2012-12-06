// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/magnification_manager.h"

#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
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

namespace {
static MagnificationManager* g_magnification_manager = NULL;
}

class MagnificationManagerImpl : public MagnificationManager,
                                 public content::NotificationObserver {
 public:
  MagnificationManagerImpl() : profile_(NULL),
                               type_(ash::MAGNIFIER_OFF) {
    registrar_.Add(this,
                  chrome::NOTIFICATION_SESSION_STARTED,
                  content::NotificationService::AllSources());
    registrar_.Add(this,
                  chrome::NOTIFICATION_PROFILE_DESTROYED,
                  content::NotificationService::AllSources());
    registrar_.Add(this,
                  chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE,
                  content::NotificationService::AllSources());
  }

  virtual ~MagnificationManagerImpl() {
    CHECK(this == g_magnification_manager);
  }

  // MagnificationManager implimentation:
  ash::MagnifierType GetMagnifierType() OVERRIDE {
    return type_;
  }

  void SetMagnifier(ash::MagnifierType type) OVERRIDE {
    if (type == type_)
      return;

    type_ = type;

    if (profile_) {
      PrefService* prefs = profile_->GetPrefs();
      if (prefs) {
        std::string typeString =
            accessibility::ScreenMagnifierNameFromType(type);
        if (typeString != prefs->GetString(prefs::kMagnifierType)) {
          prefs->SetString(prefs::kMagnifierType, typeString);
          prefs->CommitPendingWrite();
        }
      }
    }

    ash::Shell::GetInstance()->magnification_controller()->SetEnabled(
        type == ash::MAGNIFIER_FULL);
    ash::Shell::GetInstance()->partial_magnification_controller()->SetEnabled(
        type == ash::MAGNIFIER_PARTIAL);

    NotifyMagnifierTypeChanged(type);
  }

  void AddObserver(MagnificationObserver* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(MagnificationObserver* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

 private:
  void NotifyMagnifierTypeChanged(ash::MagnifierType new_type) {
    FOR_EACH_OBSERVER(MagnificationObserver, observers_,
                      OnMagnifierTypeChanged(new_type));
  }

  ash::MagnifierType GetMagnifierTypeFromPref() {
    if (!profile_)
      return ash::MAGNIFIER_OFF;

    PrefService* prefs = profile_->GetPrefs();
    if (!prefs)
      return ash::MAGNIFIER_OFF;

    return accessibility::MagnifierTypeFromName(
        prefs->GetString(prefs::kMagnifierType).c_str());
  }

  void SetProfile(Profile* profile) {
    if (pref_change_registrar_) {
      pref_change_registrar_.reset();
    }

    if (profile) {
      pref_change_registrar_.reset(new PrefChangeRegistrar);
      pref_change_registrar_->Init(profile->GetPrefs());
      pref_change_registrar_->Add(
          prefs::kMagnifierType,
          base::Bind(&MagnificationManagerImpl::UpdateMagnifierStatus,
                     base::Unretained(this)));
    }

    profile_ = profile;
    UpdateMagnifierStatus();
  }

  void UpdateMagnifierStatus() {
    ash::MagnifierType type = GetMagnifierTypeFromPref();
    SetMagnifier(type);
  }

  // content::NotificationObserver implimentation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE:
      case chrome::NOTIFICATION_SESSION_STARTED: {
        Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
        SetProfile(profile);
        break;
      }
      case chrome::NOTIFICATION_PROFILE_DESTROYED: {
        SetProfile(NULL);
        break;
      }
    }
  }

  Profile* profile_;
  ash::MagnifierType type_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<PrefChangeRegistrar> pref_change_registrar_;

  ObserverList<MagnificationObserver> observers_;
  DISALLOW_COPY_AND_ASSIGN(MagnificationManagerImpl);
};

// static
void MagnificationManager::Initialize() {
  CHECK(g_magnification_manager == NULL);
  g_magnification_manager = new MagnificationManagerImpl();
}

// static
void MagnificationManager::Shutdown() {
  CHECK(g_magnification_manager);
  delete g_magnification_manager;
  g_magnification_manager = NULL;
}

// static
MagnificationManager* MagnificationManager::Get() {
  return g_magnification_manager;
}

}  // namespace chromeos
