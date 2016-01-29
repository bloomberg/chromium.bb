// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "ash/accelerators/magnifier_key_scroller.h"
#include "ash/accelerators/spoken_feedback_toggler.h"
#include "ash/accessibility_delegate.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/background/ash_user_wallpaper_delegate.h"
#include "chrome/browser/chromeos/display/display_configuration_observer.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/chromeos/policy/display_rotation_default_handler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_error_notifier_factory_ash.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/sync/sync_error_notifier_factory_ash.h"
#include "chrome/browser/ui/ash/chrome_new_window_delegate_chromeos.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/media_delegate_chromeos.h"
#include "chrome/browser/ui/ash/session_state_delegate_chromeos.h"
#include "chrome/browser/ui/ash/system_tray_delegate_chromeos.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "ui/aura/window.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void InitAfterFirstSessionStart() {
  // Restore focus after the user session is started.  It's needed because some
  // windows can be opened in background while login UI is still active because
  // we currently restore browser windows before login UI is deleted.
  ash::Shell* shell = ash::Shell::GetInstance();
  ash::MruWindowTracker::WindowList mru_list =
      shell->mru_window_tracker()->BuildMruWindowList();
  if (!mru_list.empty())
    mru_list.front()->Focus();

  // Enable magnifier scroll keys as there may be no mouse cursor in kiosk mode.
  ash::MagnifierKeyScroller::SetEnabled(chrome::IsRunningInForcedAppMode());

  // Enable long press action to toggle spoken feedback with hotrod
  // remote which can't handle shortcut.
  ash::SpokenFeedbackToggler::SetEnabled(chrome::IsRunningInForcedAppMode());
}

class AccessibilityDelegateImpl : public ash::AccessibilityDelegate {
 public:
  AccessibilityDelegateImpl() {
    ash::Shell::GetInstance()->AddShellObserver(
        chromeos::AccessibilityManager::Get());
  }
  ~AccessibilityDelegateImpl() override {
    ash::Shell::GetInstance()->RemoveShellObserver(
        chromeos::AccessibilityManager::Get());
  }

  void ToggleHighContrast() override {
    DCHECK(chromeos::AccessibilityManager::Get());
    chromeos::AccessibilityManager::Get()->EnableHighContrast(
        !chromeos::AccessibilityManager::Get()->IsHighContrastEnabled());
  }

  bool IsSpokenFeedbackEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
  }

  void ToggleSpokenFeedback(
      ui::AccessibilityNotificationVisibility notify) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    chromeos::AccessibilityManager::Get()->ToggleSpokenFeedback(notify);
  }

  bool IsHighContrastEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsHighContrastEnabled();
  }

  void SetMagnifierEnabled(bool enabled) override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->SetMagnifierEnabled(enabled);
  }

  void SetMagnifierType(ui::MagnifierType type) override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->SetMagnifierType(type);
  }

  bool IsMagnifierEnabled() const override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->IsMagnifierEnabled();
  }

  ui::MagnifierType GetMagnifierType() const override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->GetMagnifierType();
  }

  void SetLargeCursorEnabled(bool enabled) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->EnableLargeCursor(enabled);
  }

  bool IsLargeCursorEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsLargeCursorEnabled();
  }

  void SetAutoclickEnabled(bool enabled) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->EnableAutoclick(enabled);
  }

  bool IsAutoclickEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsAutoclickEnabled();
  }

  void SetVirtualKeyboardEnabled(bool enabled) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->
        EnableVirtualKeyboard(enabled);
  }

  bool IsVirtualKeyboardEnabled() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsVirtualKeyboardEnabled();
  }

  bool ShouldShowAccessibilityMenu() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->
        ShouldShowAccessibilityMenu();
  }

  bool IsBrailleDisplayConnected() const override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->IsBrailleDisplayConnected();
  }

  void SilenceSpokenFeedback() const override {
    TtsController::GetInstance()->Stop();
  }

  void SaveScreenMagnifierScale(double scale) override {
    if (chromeos::MagnificationManager::Get())
      chromeos::MagnificationManager::Get()->SaveScreenMagnifierScale(scale);
  }

  double GetSavedScreenMagnifierScale() override {
    if (chromeos::MagnificationManager::Get()) {
      return chromeos::MagnificationManager::Get()->
          GetSavedScreenMagnifierScale();
    }
    return std::numeric_limits<double>::min();
  }

  void TriggerAccessibilityAlert(ui::AccessibilityAlert alert) override {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    if (profile) {
      switch (alert) {
        case ui::A11Y_ALERT_WINDOW_NEEDED: {
          AutomationManagerAura::GetInstance()->HandleAlert(
              profile, l10n_util::GetStringUTF8(IDS_A11Y_ALERT_WINDOW_NEEDED));
          break;
        }
        case ui::A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED: {
          AutomationManagerAura::GetInstance()->HandleAlert(
              profile, l10n_util::GetStringUTF8(
                           IDS_A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED));
          break;
        }
        case ui::A11Y_ALERT_NONE:
          break;
      }
    }
  }

  ui::AccessibilityAlert GetLastAccessibilityAlert() override {
    return ui::A11Y_ALERT_NONE;
  }

  void PlayEarcon(int sound_key) override {
    DCHECK(chromeos::AccessibilityManager::Get());
    return chromeos::AccessibilityManager::Get()->PlayEarcon(sound_key);
  }

  base::TimeDelta PlayShutdownSound() const override {
    return chromeos::AccessibilityManager::Get()->PlayShutdownSound();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityDelegateImpl);
};

}  // anonymous namespace

bool ChromeShellDelegate::IsFirstRunAfterBoot() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kFirstExecAfterBoot);
}

void ChromeShellDelegate::PreInit() {
  chromeos::LoadDisplayPreferences(IsFirstRunAfterBoot());
  // Object owns itself, and deletes itself when Observer::OnShutdown is called:
  new policy::DisplayRotationDefaultHandler();
  // Set the observer now so that we can save the initial state
  // in Shell::Init.
  display_configuration_observer_.reset(
      new chromeos::DisplayConfigurationObserver());

  chrome_user_metrics_recorder_.reset(new ChromeUserMetricsRecorder);
}

void ChromeShellDelegate::PreShutdown() {
  display_configuration_observer_.reset();

  chrome_user_metrics_recorder_.reset();
}

ash::SessionStateDelegate* ChromeShellDelegate::CreateSessionStateDelegate() {
  return new SessionStateDelegateChromeos;
}

ash::AccessibilityDelegate* ChromeShellDelegate::CreateAccessibilityDelegate() {
  return new AccessibilityDelegateImpl;
}

ash::NewWindowDelegate* ChromeShellDelegate::CreateNewWindowDelegate() {
  return new ChromeNewWindowDelegateChromeos;
}

ash::MediaDelegate* ChromeShellDelegate::CreateMediaDelegate() {
  return new MediaDelegateChromeOS;
}

ash::SystemTrayDelegate* ChromeShellDelegate::CreateSystemTrayDelegate() {
  return chromeos::CreateSystemTrayDelegate();
}

ash::UserWallpaperDelegate* ChromeShellDelegate::CreateUserWallpaperDelegate() {
  return chromeos::CreateUserWallpaperDelegate();
}

void ChromeShellDelegate::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED: {
      Profile* profile = content::Details<Profile>(details).ptr();
      if (!chromeos::ProfileHelper::IsSigninProfile(profile) &&
          !profile->IsGuestSession() && !profile->IsSupervised()) {
        // Start the error notifier services to show auth/sync notifications.
        SigninErrorNotifierFactory::GetForProfile(profile);
        SyncErrorNotifierFactory::GetForProfile(profile);
      }
      // Do not use chrome::NOTIFICATION_PROFILE_ADDED because the
      // profile is not fully initialized by user_manager.  Use
      // chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED instead.
      if (shelf_delegate_)
        shelf_delegate_->OnUserProfileReadyToSwitch(profile);
      ash::Shell::GetInstance()->OnLoginUserProfilePrepared();
      break;
    }
    case chrome::NOTIFICATION_SESSION_STARTED:
      // InitAfterFirstSessionStart() should only be called once upon system
      // start.
      if (user_manager::UserManager::Get()->GetLoggedInUsers().size() < 2)
        InitAfterFirstSessionStart();
      ash::Shell::GetInstance()->ShowShelf();
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

void ChromeShellDelegate::PlatformInit() {
  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_SESSION_STARTED,
                 content::NotificationService::AllSources());
}
