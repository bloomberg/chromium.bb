// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include <vector>

#include "ash/accessibility_delegate.h"
#include "ash/magnifier/magnifier_constants.h"
#include "ash/media_delegate.h"
#include "ash/system/tray/default_system_tray_delegate.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "chrome/browser/accessibility/accessibility_events.h"
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

#if defined(OS_WIN)
#include "chrome/browser/ui/ash/system_tray_delegate_win.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/ash/system_tray_delegate_linux.h"
#endif

namespace {

class NewWindowDelegateImpl : public ChromeNewWindowDelegate {
 public:
  NewWindowDelegateImpl() {}
  virtual ~NewWindowDelegateImpl() {}

  // Overridden from ash::NewWindowDelegate:
  virtual void OpenFileManager() OVERRIDE {}
  virtual void OpenCrosh() OVERRIDE {}
  virtual void ShowKeyboardOverlay() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NewWindowDelegateImpl);
};

class MediaDelegateImpl : public ash::MediaDelegate {
 public:
  MediaDelegateImpl() {}
  virtual ~MediaDelegateImpl() {}
  virtual void HandleMediaNextTrack() OVERRIDE {}
  virtual void HandleMediaPlayPause() OVERRIDE {}
  virtual void HandleMediaPrevTrack() OVERRIDE {}
  virtual ash::MediaCaptureState GetMediaCaptureState(
      content::BrowserContext* context) OVERRIDE {
    return ash::MEDIA_CAPTURE_NONE;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaDelegateImpl);
};

class EmptyAccessibilityDelegate : public ash::AccessibilityDelegate {
 public:
  EmptyAccessibilityDelegate() {}
  virtual ~EmptyAccessibilityDelegate() {}

  virtual void ToggleHighContrast() OVERRIDE {
  }

  virtual bool IsHighContrastEnabled() const OVERRIDE {
    return false;
  }

  virtual bool IsSpokenFeedbackEnabled() const OVERRIDE {
    return false;
  }

  virtual void ToggleSpokenFeedback(
      ash::AccessibilityNotificationVisibility notify) OVERRIDE {
  }

  virtual void SetLargeCursorEnabled(bool enalbed) OVERRIDE {
  }

  virtual bool IsLargeCursorEnabled() const OVERRIDE {
    return false;
  }

  virtual void SetMagnifierEnabled(bool enabled) OVERRIDE {
  }

  virtual void SetMagnifierType(ash::MagnifierType type) OVERRIDE {
  }

  virtual bool IsMagnifierEnabled() const OVERRIDE {
    return false;
  }

  virtual void SetAutoclickEnabled(bool enabled) OVERRIDE {
  }

  virtual bool IsAutoclickEnabled() const OVERRIDE {
    return false;
  }

  virtual ash::MagnifierType GetMagnifierType() const OVERRIDE {
    return ash::kDefaultMagnifierType;
  }

  virtual void SaveScreenMagnifierScale(double scale) OVERRIDE {
  }

  virtual double GetSavedScreenMagnifierScale() OVERRIDE {
    return std::numeric_limits<double>::min();
  }

  virtual bool ShouldShowAccessibilityMenu() const OVERRIDE {
    return false;
  }

  virtual bool IsBrailleDisplayConnected() const OVERRIDE { return false; }

  virtual void SilenceSpokenFeedback() const OVERRIDE {
  }

  virtual void SetVirtualKeyboardEnabled(bool enabled) OVERRIDE {
  }

  virtual bool IsVirtualKeyboardEnabled() const OVERRIDE {
    return false;
  }

  virtual void TriggerAccessibilityAlert(
      ash::AccessibilityAlert alert) OVERRIDE {
  }

  virtual ash::AccessibilityAlert GetLastAccessibilityAlert() OVERRIDE {
    return ash::A11Y_ALERT_NONE;
  }

  virtual base::TimeDelta PlayShutdownSound() const OVERRIDE {
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
#if defined(OS_WIN)
  return CreateWindowsSystemTrayDelegate();
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
  return CreateLinuxSystemTrayDelegate();
#else
  return new ash::DefaultSystemTrayDelegate;
#endif
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
      if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWindows8Search))
        break;
#endif
      // If Chrome ASH is launched when no browser is open in the desktop,
      // we should execute the startup code.
      // If there are browsers open in the desktop, we create a browser window
      // and open a new tab page, if session restore is not on.
      BrowserList* desktop_list = BrowserList::GetInstance(
          chrome::HOST_DESKTOP_TYPE_NATIVE);
      if (desktop_list->empty()) {
        // We pass a dummy command line here, because the browser is launched in
        // silent-mode by the metro viewer process, which causes the
        // StartupBrowserCreatorImpl class to not create any browsers which is
        // not the behavior we want.
        CommandLine dummy(CommandLine::NO_PROGRAM);
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
