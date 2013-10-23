// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "base/command_line.h"
#include "ash/magnifier/magnifier_constants.h"
#include "ash/system/tray/default_system_tray_delegate.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/caps_lock_delegate_views.h"
#include "chrome/browser/ui/ash/session_state_delegate_views.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/ash/user_wallpaper_delegate_win.h"
#endif

bool ChromeShellDelegate::IsFirstRunAfterBoot() const {
  return false;
}

void ChromeShellDelegate::PreInit() {
}

void ChromeShellDelegate::Shutdown() {
}

void ChromeShellDelegate::OpenFileManager() {
}

void ChromeShellDelegate::OpenCrosh() {
}

void ChromeShellDelegate::ShowKeyboardOverlay() {
}

void ChromeShellDelegate::ToggleHighContrast() {
}

bool ChromeShellDelegate::IsSpokenFeedbackEnabled() const {
  return false;
}

void ChromeShellDelegate::ToggleSpokenFeedback(
    ash::AccessibilityNotificationVisibility notify) {
}

void ChromeShellDelegate::SetLargeCursorEnabled(bool enalbed) {
}

bool ChromeShellDelegate::IsLargeCursorEnabled() const {
  return false;
}

void ChromeShellDelegate::SetAutoclickEnabled(bool enabled) {
}

bool ChromeShellDelegate::IsAutoclickEnabled() const {
  return false;
}

bool ChromeShellDelegate::IsHighContrastEnabled() const {
  return false;
}

void ChromeShellDelegate::SetMagnifierEnabled(bool enabled) {
}

void ChromeShellDelegate::SetMagnifierType(ash::MagnifierType type) {
}

bool ChromeShellDelegate::IsMagnifierEnabled() const {
  return false;
}

ash::MagnifierType ChromeShellDelegate::GetMagnifierType() const {
  return ash::kDefaultMagnifierType;
}

ash::CapsLockDelegate* ChromeShellDelegate::CreateCapsLockDelegate() {
  return new CapsLockDelegate();
}

ash::SessionStateDelegate* ChromeShellDelegate::CreateSessionStateDelegate() {
  return new SessionStateDelegate;
}

void ChromeShellDelegate::SaveScreenMagnifierScale(double scale) {
}

double ChromeShellDelegate::GetSavedScreenMagnifierScale() {
  return std::numeric_limits<double>::min();
}

bool ChromeShellDelegate::ShouldAlwaysShowAccessibilityMenu() const {
  return false;
}

void ChromeShellDelegate::SilenceSpokenFeedback() const {
}

ash::SystemTrayDelegate* ChromeShellDelegate::CreateSystemTrayDelegate() {
  // TODO(sky): need to subclass and override Shutdown() in a meaningful way.
  return new ash::DefaultSystemTrayDelegate;
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
        Browser* browser = GetTargetBrowser();
        chrome::AddBlankTabAt(browser, -1, true);
        browser->window()->Show();
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
