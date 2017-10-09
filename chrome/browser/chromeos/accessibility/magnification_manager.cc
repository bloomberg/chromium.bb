// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/magnification_manager.h"

#include <limits>
#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility_types.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/shell.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace chromeos {

namespace {
MagnificationManager* g_magnification_manager = nullptr;
}

class MagnificationManagerImpl
    : public MagnificationManager,
      public content::NotificationObserver,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  MagnificationManagerImpl()
      : profile_(NULL),
        magnifier_enabled_pref_handler_(
            ash::prefs::kAccessibilityScreenMagnifierEnabled),
        magnifier_scale_pref_handler_(
            ash::prefs::kAccessibilityScreenMagnifierScale),
        enabled_(false),
        keep_focus_centered_(false),
        observing_focus_change_in_page_(false) {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_SESSION_STARTED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                   content::NotificationService::AllSources());
  }

  ~MagnificationManagerImpl() override {
    CHECK(this == g_magnification_manager);
  }

  // MagnificationManager implimentation:
  bool IsMagnifierEnabled() const override { return enabled_; }

  void SetMagnifierEnabled(bool enabled) override {
    if (!profile_)
      return;

    PrefService* prefs = profile_->GetPrefs();
    prefs->SetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled,
                      enabled);
    prefs->CommitPendingWrite();
  }

  void SaveScreenMagnifierScale(double scale) override {
    if (!profile_)
      return;

    profile_->GetPrefs()->SetDouble(
        ash::prefs::kAccessibilityScreenMagnifierScale, scale);
  }

  double GetSavedScreenMagnifierScale() const override {
    if (!profile_)
      return std::numeric_limits<double>::min();

    return profile_->GetPrefs()->GetDouble(
        ash::prefs::kAccessibilityScreenMagnifierScale);
  }

  void SetProfileForTest(Profile* profile) override { SetProfile(profile); }

  // user_manager::UserManager::UserSessionStateObserver overrides:
  void ActiveUserChanged(const user_manager::User* active_user) override {
    if (active_user && active_user->is_profile_created())
      SetProfile(ProfileManager::GetActiveUserProfile());
  }

 private:
  void SetProfile(Profile* profile) {
    pref_change_registrar_.reset();

    if (profile) {
      // TODO(yoshiki): Move following code to PrefHandler.
      pref_change_registrar_.reset(new PrefChangeRegistrar);
      pref_change_registrar_->Init(profile->GetPrefs());
      pref_change_registrar_->Add(
          ash::prefs::kAccessibilityScreenMagnifierEnabled,
          base::Bind(&MagnificationManagerImpl::UpdateMagnifierFromPrefs,
                     base::Unretained(this)));
      pref_change_registrar_->Add(
          ash::prefs::kAccessibilityScreenMagnifierCenterFocus,
          base::Bind(&MagnificationManagerImpl::UpdateMagnifierFromPrefs,
                     base::Unretained(this)));
    }

    magnifier_enabled_pref_handler_.HandleProfileChanged(profile_, profile);
    magnifier_scale_pref_handler_.HandleProfileChanged(profile_, profile);

    profile_ = profile;
    UpdateMagnifierFromPrefs();
  }

  virtual void SetMagnifierEnabledInternal(bool enabled) {
    // This method may be invoked even when the other magnifier settings (e.g.
    // type or scale) are changed, so we need to call magnification controller
    // even if |enabled| is unchanged. Only if |enabled| is false and the
    // magnifier is already disabled, we are sure that we don't need to reflect
    // the new settings right now because the magnifier keeps disabled.
    if (!enabled && !enabled_)
      return;

    enabled_ = enabled;

    ash::Shell::Get()->magnification_controller()->SetEnabled(enabled_);
    MonitorFocusInPageChange();
  }

  virtual void SetMagniferKeepFocusCenteredInternal(bool keep_focus_centered) {
    if (keep_focus_centered_ == keep_focus_centered)
      return;

    keep_focus_centered_ = keep_focus_centered;

    ash::Shell::Get()->magnification_controller()->SetKeepFocusCentered(
        keep_focus_centered_);
  }

  void UpdateMagnifierFromPrefs() {
    if (!profile_)
      return;

    PrefService* prefs = profile_->GetPrefs();
    const bool enabled =
        prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled);
    const bool keep_focus_centered =
        prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierCenterFocus);

    if (!enabled) {
      SetMagnifierEnabledInternal(enabled);
      SetMagniferKeepFocusCenteredInternal(keep_focus_centered);
    } else {
      SetMagniferKeepFocusCenteredInternal(keep_focus_centered);
      SetMagnifierEnabledInternal(enabled);
    }

    AccessibilityStatusEventDetails details(
        ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER, enabled_,
        ash::A11Y_NOTIFICATION_NONE);

    if (AccessibilityManager::Get()) {
      AccessibilityManager::Get()->NotifyAccessibilityStatusChanged(details);
      if (ash::Shell::Get()) {
        ash::Shell::Get()->SetCursorCompositingEnabled(
            ash::AccessibilityController::RequiresCursorCompositing(prefs));
      }
    }
  }

  void MonitorFocusInPageChange() {
    if (enabled_ && !observing_focus_change_in_page_) {
      registrar_.Add(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                     content::NotificationService::AllSources());
      observing_focus_change_in_page_ = true;
    } else if (!enabled_ && observing_focus_change_in_page_) {
      registrar_.Remove(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                        content::NotificationService::AllSources());
      observing_focus_change_in_page_ = false;
    }
  }

  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    switch (type) {
      case chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE: {
        // Update |profile_| when entering the login screen.
        Profile* profile = ProfileManager::GetActiveUserProfile();
        if (ProfileHelper::IsSigninProfile(profile))
          SetProfile(profile);
        break;
      }
      case chrome::NOTIFICATION_SESSION_STARTED:
        // Update |profile_| when entering a session.
        SetProfile(ProfileManager::GetActiveUserProfile());

        // Add a session state observer to be able to monitor session changes.
        if (!session_state_observer_.get())
          session_state_observer_.reset(
              new user_manager::ScopedUserSessionStateObserver(this));
        break;
      case chrome::NOTIFICATION_PROFILE_DESTROYED: {
        // Update |profile_| when exiting a session or shutting down.
        Profile* profile = content::Source<Profile>(source).ptr();
        if (profile_ == profile)
          SetProfile(NULL);
        break;
      }
      case content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE: {
        content::FocusedNodeDetails* node_details =
            content::Details<content::FocusedNodeDetails>(details).ptr();
        ash::Shell::Get()->magnification_controller()->HandleFocusedNodeChanged(
            node_details->is_editable_node,
            node_details->node_bounds_in_screen);
        break;
      }
    }
  }

  Profile* profile_;

  AccessibilityManager::PrefHandler magnifier_enabled_pref_handler_;
  AccessibilityManager::PrefHandler magnifier_scale_pref_handler_;

  bool enabled_;
  bool keep_focus_centered_;
  bool observing_focus_change_in_page_;

  content::NotificationRegistrar registrar_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<user_manager::ScopedUserSessionStateObserver>
      session_state_observer_;

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
