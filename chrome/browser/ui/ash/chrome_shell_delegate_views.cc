// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include <vector>

#include "ash/accessibility_delegate.h"
#include "ash/media_delegate.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_error_notifier_factory_ash.h"
#include "chrome/browser/sync/sync_error_notifier_factory_ash.h"
#include "chrome/browser/ui/ash/chrome_new_window_delegate.h"
#include "chrome/browser/ui/ash/session_state_delegate_views.h"
#include "chrome/browser/ui/ash/solid_color_user_wallpaper_delegate.h"
#include "chrome/browser/ui/ash/system_tray_delegate_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "ui/chromeos/accessibility_types.h"

namespace {

class NewWindowDelegateImpl : public ChromeNewWindowDelegate {
 public:
  NewWindowDelegateImpl() {}
  ~NewWindowDelegateImpl() override {}

  // Overridden from ash::NewWindowDelegate:
  void OpenFileManager() override {}
  void OpenCrosh() override {}
  void OpenGetHelp() override {}
  void ShowKeyboardOverlay() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NewWindowDelegateImpl);
};

class MediaDelegateImpl : public ash::MediaDelegate {
 public:
  MediaDelegateImpl() {}
  ~MediaDelegateImpl() override {}
  void HandleMediaNextTrack() override {}
  void HandleMediaPlayPause() override {}
  void HandleMediaPrevTrack() override {}
  ash::MediaCaptureState GetMediaCaptureState(ash::UserIndex index) override {
    return ash::MEDIA_CAPTURE_NONE;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaDelegateImpl);
};

class EmptyAccessibilityDelegate : public ash::AccessibilityDelegate {
 public:
  EmptyAccessibilityDelegate() {}
  ~EmptyAccessibilityDelegate() override {}

  void ToggleHighContrast() override {}

  bool IsHighContrastEnabled() const override { return false; }

  bool IsSpokenFeedbackEnabled() const override { return false; }

  void ToggleSpokenFeedback(
      ui::AccessibilityNotificationVisibility notify) override {}

  void SetLargeCursorEnabled(bool enalbed) override {}

  bool IsLargeCursorEnabled() const override { return false; }

  void SetMagnifierEnabled(bool enabled) override {}

  void SetMagnifierType(ui::MagnifierType type) override {}

  bool IsMagnifierEnabled() const override { return false; }

  void SetAutoclickEnabled(bool enabled) override {}

  bool IsAutoclickEnabled() const override { return false; }

  ui::MagnifierType GetMagnifierType() const override {
    return ui::kDefaultMagnifierType;
  }

  void SaveScreenMagnifierScale(double scale) override {}

  double GetSavedScreenMagnifierScale() override {
    return std::numeric_limits<double>::min();
  }

  bool ShouldShowAccessibilityMenu() const override { return false; }

  bool IsBrailleDisplayConnected() const override { return false; }

  void SilenceSpokenFeedback() const override {}

  void SetVirtualKeyboardEnabled(bool enabled) override {}

  bool IsVirtualKeyboardEnabled() const override { return false; }

  void TriggerAccessibilityAlert(ui::AccessibilityAlert alert) override {}

  ui::AccessibilityAlert GetLastAccessibilityAlert() override {
    return ui::A11Y_ALERT_NONE;
  }

  void PlayEarcon(int sound_key) override {}

  base::TimeDelta PlayShutdownSound() const override {
    return base::TimeDelta();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyAccessibilityDelegate);
};

}  // namespace

bool ChromeShellDelegate::IsFirstRunAfterBoot() const {
  return false;
}

void ChromeShellDelegate::PreInit() {
}

void ChromeShellDelegate::PreShutdown() {
}

ash::NewWindowDelegate* ChromeShellDelegate::CreateNewWindowDelegate() {
  return new NewWindowDelegateImpl;
}

ash::MediaDelegate* ChromeShellDelegate::CreateMediaDelegate() {
  return new MediaDelegateImpl;
}

ash::SessionStateDelegate* ChromeShellDelegate::CreateSessionStateDelegate() {
  return new SessionStateDelegate;
}

ash::SystemTrayDelegate* ChromeShellDelegate::CreateSystemTrayDelegate() {
  return new SystemTrayDelegateCommon();
}

ash::AccessibilityDelegate* ChromeShellDelegate::CreateAccessibilityDelegate() {
  return new EmptyAccessibilityDelegate;
}

ash::UserWallpaperDelegate* ChromeShellDelegate::CreateUserWallpaperDelegate() {
  return CreateSolidColorUserWallpaperDelegate();
}

void ChromeShellDelegate::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_ADDED: {
      // Start the error notifier services to show sync/auth notifications.
      Profile* profile = content::Source<Profile>(source).ptr();
      SigninErrorNotifierFactory::GetForProfile(profile);
      SyncErrorNotifierFactory::GetForProfile(profile);
      break;
    }
    case chrome::NOTIFICATION_ASH_SESSION_STARTED: {
      // Start the error notifier services for the already loaded profiles.
      const std::vector<Profile*> profiles =
          g_browser_process->profile_manager()->GetLoadedProfiles();
      for (std::vector<Profile*>::const_iterator it = profiles.begin();
           it != profiles.end(); ++it) {
        SigninErrorNotifierFactory::GetForProfile(*it);
        SyncErrorNotifierFactory::GetForProfile(*it);
      }

#if defined(OS_WIN)
      // If we are launched to service a windows 8 search request then let the
      // IPC which carries the search string create the browser and initiate
      // the navigation.
      if (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kWindows8Search))
        break;
#endif
      // If Chrome ASH is launched when no browser is open in the desktop,
      // we should execute the startup code.
      // If there are browsers open in the desktop, we create a browser window
      // and open a new tab page, if session restore is not on.
      BrowserList* desktop_list = BrowserList::GetInstance();
      if (desktop_list->empty()) {
        // We pass a dummy command line here, because the browser is launched in
        // silent-mode by the metro viewer process, which causes the
        // StartupBrowserCreatorImpl class to not create any browsers which is
        // not the behavior we want.
        base::CommandLine dummy(base::CommandLine::NO_PROGRAM);
        StartupBrowserCreatorImpl startup_impl(
            base::FilePath(),
            dummy,
            chrome::startup::IS_NOT_FIRST_RUN);
        startup_impl.Launch(
            ProfileManager::GetActiveUserProfile(),
            std::vector<GURL>(),
            true,
            chrome::HOST_DESKTOP_TYPE_ASH);
      } else {
        Browser* browser =
            chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
        if (browser && browser->is_type_tabbed()) {
          chrome::AddTabAt(browser, GURL(), -1, true);
          return;
        }

        chrome::ScopedTabbedBrowserDisplayer displayer(
            ProfileManager::GetActiveUserProfile(),
            chrome::HOST_DESKTOP_TYPE_ASH);
        chrome::AddTabAt(displayer.browser(), GURL(), -1, true);
      }
      break;
    }
    case chrome::NOTIFICATION_ASH_SESSION_ENDED:
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

void ChromeShellDelegate::PlatformInit() {
#if defined(OS_WIN)
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_ADDED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_ASH_SESSION_STARTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_ASH_SESSION_ENDED,
                 content::NotificationService::AllSources());
#endif
}
