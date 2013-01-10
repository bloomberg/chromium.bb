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
                               type_(ash::kDefaultMagnifierType),
                               enabled_(false) {
    registrar_.Add(this,
                  chrome::NOTIFICATION_PROFILE_CREATED,
                  content::NotificationService::AllSources());
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
  bool IsMagnifierEnabled() const OVERRIDE {
    return enabled_;
  }

  ash::MagnifierType GetMagnifierType() const OVERRIDE {
    return type_;
  }

  void SetMagnifierEnabled(bool enabled) OVERRIDE {
    // This method may be invoked even when the other magnifier settings (e.g.
    // type or scale) are changed, so we need to call magnification controller
    // even if |enabled| is unchanged. Only if |enabled| is false and the
    // magnifier is already disabled, we are sure that we don't need to reflect
    // the new settings right now because the magnifier keeps disabled.
    if (!enabled && !enabled_)
      return;

    enabled_ = enabled;

    if (profile_) {
      PrefService* prefs = profile_->GetPrefs();
      DCHECK(prefs);
      if (enabled != prefs->GetBoolean(prefs::kScreenMagnifierEnabled)) {
        prefs->SetBoolean(prefs::kScreenMagnifierEnabled, enabled);
        prefs->CommitPendingWrite();
      }
    }

    NotifyMagnifierChanged();

    if (type_ == ash::MAGNIFIER_FULL) {
      ash::Shell::GetInstance()->magnification_controller()->SetEnabled(
          enabled_);
    } else {
      ash::Shell::GetInstance()->partial_magnification_controller()->SetEnabled(
          enabled_);
    }
  }

  void SetMagnifierType(ash::MagnifierType type) OVERRIDE {
    if (type_ == type)
      return;

    DCHECK(type == ash::MAGNIFIER_FULL || type == ash::MAGNIFIER_PARTIAL);
    type_ = type;

    if (profile_) {
      PrefService* prefs = profile_->GetPrefs();
      DCHECK(prefs);
      prefs->SetInteger(prefs::kScreenMagnifierType, type);
      prefs->CommitPendingWrite();
    }

    NotifyMagnifierChanged();

    if (enabled_) {
      ash::Shell::GetInstance()->magnification_controller()->SetEnabled(
          type_ == ash::MAGNIFIER_FULL);
      ash::Shell::GetInstance()->partial_magnification_controller()->SetEnabled(
          type_ == ash::MAGNIFIER_PARTIAL);
    }
  }

  void SaveScreenMagnifierScale(double scale) OVERRIDE {
    Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    DCHECK(profile->GetPrefs());
    profile->GetPrefs()->SetDouble(prefs::kScreenMagnifierScale, scale);
  }

  double GetSavedScreenMagnifierScale() const OVERRIDE {
    Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    DCHECK(profile->GetPrefs());
    if (profile->GetPrefs()->HasPrefPath(prefs::kScreenMagnifierScale))
      return profile->GetPrefs()->GetDouble(prefs::kScreenMagnifierScale);
    return std::numeric_limits<double>::min();
  }

 private:
  void NotifyMagnifierChanged() {
      accessibility::AccessibilityStatusEventDetails details(
          enabled_, type_, ash::A11Y_NOTIFICATION_NONE);
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER,
          content::NotificationService::AllSources(),
          content::Details<accessibility::AccessibilityStatusEventDetails>(
              &details));
  }

  bool IsMagnifierEnabledFromPref() {
    if (!profile_)
      return false;

    DCHECK(profile_->GetPrefs());
    return profile_->GetPrefs()->GetBoolean(prefs::kScreenMagnifierEnabled);
  }

  ash::MagnifierType GetMagnifierTypeFromPref() {
    if (!profile_)
      return ash::kDefaultMagnifierType;

    DCHECK(profile_->GetPrefs());
    ash::MagnifierType type = static_cast<ash::MagnifierType>(
        profile_->GetPrefs()->GetInteger(prefs::kScreenMagnifierType));

    if (type == ash::MAGNIFIER_FULL || type == ash::MAGNIFIER_PARTIAL)
      return type;

    return ash::kDefaultMagnifierType;
  }

  void SetProfile(Profile* profile) {
    if (pref_change_registrar_) {
      pref_change_registrar_.reset();
    }

    if (profile) {
      pref_change_registrar_.reset(new PrefChangeRegistrar);
      pref_change_registrar_->Init(profile->GetPrefs());
      pref_change_registrar_->Add(
          prefs::kScreenMagnifierEnabled,
          base::Bind(&MagnificationManagerImpl::UpdateMagnifierStatusFromPref,
                     base::Unretained(this)));
      pref_change_registrar_->Add(
          prefs::kScreenMagnifierType,
          base::Bind(&MagnificationManagerImpl::UpdateMagnifierStatusFromPref,
                     base::Unretained(this)));
    }

    profile_ = profile;
    UpdateMagnifierStatusFromPref();
  }

  void UpdateMagnifierStatusFromPref() {
    bool enabled = IsMagnifierEnabledFromPref();
    if (!enabled) {
      SetMagnifierEnabled(enabled);
      SetMagnifierType(GetMagnifierTypeFromPref());
    } else {
      SetMagnifierType(GetMagnifierTypeFromPref());
      SetMagnifierEnabled(enabled);
    }
  }

  // content::NotificationObserver implimentation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      // When entering the login screen or non-guest desktop.
      case chrome::NOTIFICATION_LOGIN_WEBUI_VISIBLE:
      case chrome::NOTIFICATION_SESSION_STARTED: {
        Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
        if (!profile->IsGuestSession())
          SetProfile(profile);
        break;
      }
      // When entering guest desktop, no NOTIFICATION_SESSION_STARTED event is
      // fired, so we use NOTIFICATION_PROFILE_CREATED event instead.
      case chrome::NOTIFICATION_PROFILE_CREATED: {
        Profile* profile = content::Source<Profile>(source).ptr();
        if (profile->IsGuestSession())
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
  bool enabled_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<PrefChangeRegistrar> pref_change_registrar_;

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
