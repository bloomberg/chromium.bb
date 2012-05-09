// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/chrome_shell_delegate.h"

#include "ash/launcher/launcher_types.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/wm/partial_screenshot_view.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/ash/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/views/ash/window_positioner.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/aura/window.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/background/ash_user_wallpaper_delegate.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/webui_login_display_host.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system/ash_system_tray_delegate.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_dialog.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#endif

// static
ChromeShellDelegate* ChromeShellDelegate::instance_ = NULL;

ChromeShellDelegate::ChromeShellDelegate()
    : window_positioner_(new WindowPositioner()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  instance_ = this;
#if defined(OS_CHROMEOS)
  registrar_.Add(
      this,
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources());
#endif
}

ChromeShellDelegate::~ChromeShellDelegate() {
  if (instance_ == this)
    instance_ = NULL;
}

bool ChromeShellDelegate::IsUserLoggedIn() {
#if defined(OS_CHROMEOS)
  // When running a Chrome OS build outside of a device (i.e. on a developer's
  // workstation), pretend like we're always logged in.
  if (!base::chromeos::IsRunningOnChromeOS())
    return true;

  return chromeos::UserManager::Get()->IsUserLoggedIn();
#else
  return true;
#endif
}

void ChromeShellDelegate::LockScreen() {
#if defined(OS_CHROMEOS)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession) &&
      !chromeos::KioskModeSettings::Get()->IsKioskModeEnabled()) {
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
        NotifyScreenLockRequested();
  }
#endif
}

void ChromeShellDelegate::UnlockScreen() {
  // This is used only for testing thus far.
  NOTIMPLEMENTED();
}

bool ChromeShellDelegate::IsScreenLocked() const {
#if defined(OS_CHROMEOS)
  if (!chromeos::ScreenLocker::default_screen_locker())
    return false;
  return chromeos::ScreenLocker::default_screen_locker()->locked();
#else
  return false;
#endif
}

void ChromeShellDelegate::Shutdown() {
#if defined(OS_CHROMEOS)
  content::RecordAction(content::UserMetricsAction("Shutdown"));
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestShutdown();
#endif
}

void ChromeShellDelegate::Exit() {
  BrowserList::AttemptUserExit();
}

void ChromeShellDelegate::NewWindow(bool is_incognito) {
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  Browser::NewEmptyWindow(
      is_incognito ? profile->GetOffTheRecordProfile() : profile);
}

void ChromeShellDelegate::Search() {
  // Exit fullscreen to show omnibox.
  Browser* last_active = BrowserList::GetLastActive();
  if (last_active) {
    if (last_active->window()->IsFullscreen()) {
      last_active->ToggleFullscreenMode();
      // ToggleFullscreenMode is asynchronous, so we don't have omnibox
      // visible at this point. Wait for next event cycle which toggles
      // the visibility of omnibox before creating new tab.
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&ChromeShellDelegate::Search,
                                weak_factory_.GetWeakPtr()));
      return;
    }
  }

  Browser* target_browser = Browser::GetOrCreateTabbedBrowser(
      last_active ? last_active->profile() :
                    ProfileManager::GetDefaultProfileOrOffTheRecord());
  const GURL& url = target_browser->GetSelectedWebContents() ?
      target_browser->GetSelectedWebContents()->GetURL() : GURL();
  if (url.SchemeIs(chrome::kChromeUIScheme) &&
      url.host() == chrome::kChromeUINewTabHost) {
    // If the NTP is showing, focus the omnibox.
    target_browser->window()->SetFocusToLocationBar(true);
  } else {
    target_browser->NewTab();
  }
  target_browser->window()->Show();
}

void ChromeShellDelegate::OpenFileManager() {
#if defined(OS_CHROMEOS)
  file_manager_util::OpenApplication();
#endif
}

void ChromeShellDelegate::OpenCrosh() {
#if defined(OS_CHROMEOS)
  Browser* browser = Browser::GetOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord());
  GURL crosh_url = TerminalExtensionHelper::GetCroshExtensionURL(
      browser->profile());
  if (!crosh_url.is_valid())
    return;
  browser->OpenURL(
      content::OpenURLParams(crosh_url,
                             content::Referrer(),
                             NEW_FOREGROUND_TAB,
                             content::PAGE_TRANSITION_GENERATED,
                             false));
#endif
}

void ChromeShellDelegate::OpenMobileSetup() {
#if defined(OS_CHROMEOS)
  Browser* browser = Browser::GetOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord());
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableMobileSetupDialog)) {
    MobileSetupDialog::Show();
  } else {
    browser->OpenURL(
        content::OpenURLParams(GURL(chrome::kChromeUIMobileSetupURL),
                               content::Referrer(),
                               NEW_FOREGROUND_TAB,
                               content::PAGE_TRANSITION_LINK,
                               false));
    browser->window()->Activate();
  }
#endif
}

content::BrowserContext* ChromeShellDelegate::GetCurrentBrowserContext() {
  return ProfileManager::GetDefaultProfile();
}

void ChromeShellDelegate::ToggleSpokenFeedback() {
#if defined(OS_CHROMEOS)
  content::WebUI* login_screen_web_ui = NULL;
  chromeos::WebUILoginDisplayHost* host =
      static_cast<chromeos::WebUILoginDisplayHost*>(
          chromeos::BaseLoginDisplayHost::default_host());
  if (host && host->GetOobeUI())
    login_screen_web_ui = host->GetOobeUI()->web_ui();
  chromeos::accessibility::ToggleSpokenFeedback(login_screen_web_ui);
#endif
}

app_list::AppListViewDelegate*
    ChromeShellDelegate::CreateAppListViewDelegate() {
  // Shell will own the created delegate.
  return new AppListViewDelegate;
}

void ChromeShellDelegate::StartPartialScreenshot(
    ash::ScreenshotDelegate* screenshot_delegate) {
  ash::PartialScreenshotView::StartPartialScreenshot(screenshot_delegate);
}

ash::LauncherDelegate* ChromeShellDelegate::CreateLauncherDelegate(
    ash::LauncherModel* model) {
  ChromeLauncherController* controller =
      new ChromeLauncherController(NULL, model);
  controller->Init();
  return controller;
}

ash::SystemTrayDelegate* ChromeShellDelegate::CreateSystemTrayDelegate(
    ash::SystemTray* tray) {
#if defined(OS_CHROMEOS)
  return chromeos::CreateSystemTrayDelegate(tray);
#else
  return NULL;
#endif
}

ash::UserWallpaperDelegate* ChromeShellDelegate::CreateUserWallpaperDelegate() {
#if defined(OS_CHROMEOS)
  return chromeos::CreateUserWallpaperDelegate();
#else
  return NULL;
#endif
}

void ChromeShellDelegate::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
#if defined(OS_CHROMEOS)
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED:
      ash::Shell::GetInstance()->CreateLauncher();
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
#else
  // MSVC++ warns about switch statements without any cases.
  NOTREACHED() << "Unexpected notification " << type;
#endif
}
