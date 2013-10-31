// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "ash/accessibility_delegate.h"
#include "ash/magnifier/magnifier_constants.h"
#include "ash/system/tray/default_system_tray_delegate.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/caps_lock_delegate_views.h"
#include "chrome/browser/ui/ash/chrome_new_window_delegate.h"
#include "chrome/browser/ui/ash/session_state_delegate_views.h"
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
#include "chrome/browser/ui/ash/user_wallpaper_delegate_win.h"
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

  virtual bool ShouldAlwaysShowAccessibilityMenu() const OVERRIDE {
    return false;
  }

  virtual void SilenceSpokenFeedback() const OVERRIDE {
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

void ChromeShellDelegate::Shutdown() {
}

ash::NewWindowDelegate* ChromeShellDelegate::CreateNewWindowDelegate() {
  return new NewWindowDelegateImpl;
}

ash::CapsLockDelegate* ChromeShellDelegate::CreateCapsLockDelegate() {
  return new CapsLockDelegate();
}

ash::SessionStateDelegate* ChromeShellDelegate::CreateSessionStateDelegate() {
  return new SessionStateDelegate;
}

ash::SystemTrayDelegate* ChromeShellDelegate::CreateSystemTrayDelegate() {
#if defined(OS_WIN)
  return CreateWindowsSystemTrayDelegate();
#else
  return new ash::DefaultSystemTrayDelegate;
#endif
}

ash::AccessibilityDelegate* ChromeShellDelegate::CreateAccessibilityDelegate() {
  return new EmptyAccessibilityDelegate;
}

ash::UserWallpaperDelegate* ChromeShellDelegate::CreateUserWallpaperDelegate() {
#if defined(OS_WIN)
  return ::CreateUserWallpaperDelegate();
#else
  return NULL;
#endif
}

void ChromeShellDelegate::HandleMediaNextTrack() {
}

void ChromeShellDelegate::HandleMediaPlayPause() {
}

void ChromeShellDelegate::HandleMediaPrevTrack() {
}

void ChromeShellDelegate::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_ASH_SESSION_STARTED: {
      // If we are launched to service a windows 8 search request then let the
      // IPC which carries the search string create the browser and initiate
      // the navigation.
      if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWindows8Search))
        break;
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
        startup_impl.Launch(ProfileManager::GetDefaultProfileOrOffTheRecord(),
                            std::vector<GURL>(),
                            true,
                            chrome::HOST_DESKTOP_TYPE_ASH);
      } else {
        Browser* browser =
            chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
        if (browser && browser->is_type_tabbed()) {
          chrome::AddBlankTabAt(browser, -1, true);
          return;
        }

        chrome::ScopedTabbedBrowserDisplayer displayer(
            ProfileManager::GetDefaultProfileOrOffTheRecord(),
            chrome::HOST_DESKTOP_TYPE_ASH);
        chrome::AddBlankTabAt(displayer.browser(), -1, true);
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
                 chrome::NOTIFICATION_ASH_SESSION_STARTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_ASH_SESSION_ENDED,
                 content::NotificationService::AllSources());
#endif
}
