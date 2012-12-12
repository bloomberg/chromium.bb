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
#include "base/prefs/public/pref_member.h"
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
const double kInitialMagnifiedScale = 2.0;
static MagnificationManager* g_magnification_manager = NULL;
}

class MagnificationManagerImpl : public MagnificationManager,
                                 public content::NotificationObserver {
 public:
  MagnificationManagerImpl() : first_time_update_(true),
                               profile_(NULL),
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
    if (type == type_ && type == ash::MAGNIFIER_OFF)
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

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());

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

  void SaveScreenMagnifierScale(double scale) OVERRIDE {
    Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    profile->GetPrefs()->SetDouble(prefs::kScreenMagnifierScale, scale);
  }

  double GetSavedScreenMagnifierScale() OVERRIDE {
    Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    if (profile->GetPrefs()->HasPrefPath(prefs::kScreenMagnifierScale))
      return profile->GetPrefs()->GetDouble(prefs::kScreenMagnifierScale);
    return std::numeric_limits<double>::min();
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
    // Historycally, from r162080 to r170956, screen magnifier had been enabled
    // with 1.0x scale on login screen by default, hence some users
    // unintentionally have the pref to enable magnifier. Now, the default scale
    // is 2.0x on login screen (same as other screens), so despite them, with
    // the old pref, their screen might be magnified with 2.0x scale.
    // The following code prevents it. If the user on login screen has full
    // screen magnifier pref but no scale pref, doesn't make magnifier enabled.
    // TODO(yoshiki): remove this in the near future: crbug.com/164627
    if (first_time_update_) {
      first_time_update_ = false;
      UserManager* manager = UserManager::Get();
      if (profile_ &&
          !profile_->GetPrefs()->HasPrefPath(prefs::kScreenMagnifierScale) &&
          accessibility::MagnifierTypeFromName(profile_->GetPrefs()->GetString(
              prefs::kMagnifierType).c_str()) == ash::MAGNIFIER_FULL &&
          manager &&
          !manager->IsSessionStarted()) {
        SetMagnifier(ash::MAGNIFIER_OFF);
        profile_->GetPrefs()->SetDouble(prefs::kScreenMagnifierScale,
                                        kInitialMagnifiedScale);
        return;
      }
    }

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

  bool first_time_update_;
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
