// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "ash/keyboard_overlay/keyboard_overlay_view.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/window_util.h"
#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/background/ash_user_wallpaper_delegate.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/chromeos/extensions/media_player_event_router.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/webui_login_display_host.h"
#include "chrome/browser/chromeos/system/ash_system_tray_delegate.h"
#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/caps_lock_delegate_chromeos.h"
#include "chrome/browser/ui/ash/window_positioner.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_dialog.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

bool ChromeShellDelegate::IsUserLoggedIn() const {
  // When running a Chrome OS build outside of a device (i.e. on a developer's
  // workstation) and not running as login-manager, pretend like we're always
  // logged in.
  if (!base::chromeos::IsRunningOnChromeOS() &&
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kLoginManager)) {
    return true;
  }

  return chromeos::UserManager::Get()->IsUserLoggedIn();
}

  // Returns true if we're logged in and browser has been started
bool ChromeShellDelegate::IsSessionStarted() const {
  return chromeos::UserManager::Get()->IsSessionStarted();
}

bool ChromeShellDelegate::IsFirstRunAfterBoot() const {
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kFirstBoot);
}

bool ChromeShellDelegate::CanLockScreen() const {
  return chromeos::UserManager::Get()->CanCurrentUserLock();
}

void ChromeShellDelegate::LockScreen() {
  if (CanLockScreen()) {
    // TODO(antrim) : additional logging for crbug/173178
    LOG(WARNING) << "Requesting screen lock from ChromeShellDelegate";
    chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
        RequestLockScreen();
  }
}

bool ChromeShellDelegate::IsScreenLocked() const {
  if (!chromeos::ScreenLocker::default_screen_locker())
    return false;
  return chromeos::ScreenLocker::default_screen_locker()->locked();
}

void ChromeShellDelegate::Shutdown() {
  content::RecordAction(content::UserMetricsAction("Shutdown"));
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestShutdown();
}

void ChromeShellDelegate::OpenFileManager(bool as_dialog) {
  if (as_dialog) {
    Browser* browser =
        chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
    // Open the select file dialog only if there is an active browser where the
    // selected file is displayed.
    if (browser) {
      browser->OpenFile();
      return;
    }
  } else {
    file_manager_util::OpenFileBrowser();
  }
}

void ChromeShellDelegate::OpenCrosh() {
  GURL crosh_url = TerminalExtensionHelper::GetCroshExtensionURL(
      ProfileManager::GetDefaultProfileOrOffTheRecord());
  if (!crosh_url.is_valid())
    return;
  Browser* browser = GetTargetBrowser();
  content::WebContents* page = browser->OpenURL(
      content::OpenURLParams(crosh_url,
                             content::Referrer(),
                             NEW_FOREGROUND_TAB,
                             content::PAGE_TRANSITION_GENERATED,
                             false));
  browser->window()->Show();
  browser->window()->Activate();
  page->Focus();
}

void ChromeShellDelegate::OpenMobileSetup(const std::string& service_path) {
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  const chromeos::CellularNetwork* cellular =
      cros->FindCellularNetworkByPath(service_path);
  if (cellular && cellular->activate_over_non_cellular_network() &&
      (!cros->connected_network() || !cros->connected_network()->online())) {
    chromeos::NetworkTechnology technology = cellular->network_technology();
    ash::NetworkObserver::NetworkType network_type =
        (technology == chromeos::NETWORK_TECHNOLOGY_LTE ||
         technology == chromeos::NETWORK_TECHNOLOGY_LTE_ADVANCED)
        ? ash::NetworkObserver::NETWORK_CELLULAR_LTE
        : ash::NetworkObserver::NETWORK_CELLULAR;
    ash::Shell::GetInstance()->system_tray_notifier()->NotifySetNetworkMessage(
        NULL,
        ash::NetworkObserver::ERROR_CONNECT_FAILED,
        network_type,
        l10n_util::GetStringUTF16(IDS_NETWORK_ACTIVATION_ERROR_TITLE),
        l10n_util::GetStringFUTF16(IDS_NETWORK_ACTIVATION_NEEDS_CONNECTION,
                                   UTF8ToUTF16((cellular->name()))),
        std::vector<string16>());
    return;
  }
  MobileSetupDialog::Show(service_path);
}

void ChromeShellDelegate::ToggleHighContrast() {
  bool enabled = chromeos::accessibility::IsHighContrastEnabled();
  chromeos::accessibility::EnableHighContrast(!enabled);
}

bool ChromeShellDelegate::IsSpokenFeedbackEnabled() const {
  return chromeos::accessibility::IsSpokenFeedbackEnabled();
}

void ChromeShellDelegate::ToggleSpokenFeedback(
    ash::AccessibilityNotificationVisibility notify) {
  content::WebUI* web_ui = NULL;

  chromeos::WebUILoginDisplayHost* host =
      static_cast<chromeos::WebUILoginDisplayHost*>(
          chromeos::BaseLoginDisplayHost::default_host());
  if (host && host->GetOobeUI())
    web_ui = host->GetOobeUI()->web_ui();

  if (!web_ui &&
      chromeos::ScreenLocker::default_screen_locker() &&
      chromeos::ScreenLocker::default_screen_locker()->locked()) {
    web_ui = chromeos::ScreenLocker::default_screen_locker()->
        GetAssociatedWebUI();
  }
  chromeos::accessibility::ToggleSpokenFeedback(web_ui, notify);
}

bool ChromeShellDelegate::IsHighContrastEnabled() const {
  return chromeos::accessibility::IsHighContrastEnabled();
}

bool ChromeShellDelegate::IsMagnifierEnabled() const {
  DCHECK(chromeos::MagnificationManager::Get());
  return chromeos::MagnificationManager::Get()->IsMagnifierEnabled();
}

ash::MagnifierType ChromeShellDelegate::GetMagnifierType() const {
  DCHECK(chromeos::MagnificationManager::Get());
  return chromeos::MagnificationManager::Get()->GetMagnifierType();
}

void ChromeShellDelegate::SetMagnifierEnabled(bool enabled) {
  DCHECK(chromeos::MagnificationManager::Get());
  return chromeos::MagnificationManager::Get()->SetMagnifierEnabled(enabled);
}

void ChromeShellDelegate::SetMagnifierType(ash::MagnifierType type) {
  DCHECK(chromeos::MagnificationManager::Get());
  return chromeos::MagnificationManager::Get()->SetMagnifierType(type);
}

void ChromeShellDelegate::SaveScreenMagnifierScale(double scale) {
  if (chromeos::MagnificationManager::Get())
    chromeos::MagnificationManager::Get()->SaveScreenMagnifierScale(scale);
}

double ChromeShellDelegate::GetSavedScreenMagnifierScale() {
  if (chromeos::MagnificationManager::Get()) {
    return chromeos::MagnificationManager::Get()->
        GetSavedScreenMagnifierScale();
  }
  return std::numeric_limits<double>::min();
}

ash::CapsLockDelegate* ChromeShellDelegate::CreateCapsLockDelegate() {
  chromeos::input_method::XKeyboard* xkeyboard =
      chromeos::input_method::GetInputMethodManager()->GetXKeyboard();
  return new CapsLockDelegate(xkeyboard);
}

void ChromeShellDelegate::ShowKeyboardOverlay() {
  // TODO(mazda): Move the show logic to ash (http://crbug.com/124222).
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  std::string url(chrome::kChromeUIKeyboardOverlayURL);
  ash::KeyboardOverlayView::ShowDialog(profile,
                                       new ChromeWebContentsHandler,
                                       GURL(url));
}

bool ChromeShellDelegate::ShouldAlwaysShowAccessibilityMenu() const {
  if (!IsUserLoggedIn())
    return true;

  Profile* profile = ProfileManager::GetDefaultProfile();
  if (!profile)
    return false;

  PrefService* user_pref_service = profile->GetPrefs();
  return user_pref_service &&
      user_pref_service->GetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu);
}

ash::SystemTrayDelegate* ChromeShellDelegate::CreateSystemTrayDelegate() {
  return chromeos::CreateSystemTrayDelegate();
}

ash::UserWallpaperDelegate* ChromeShellDelegate::CreateUserWallpaperDelegate() {
  return chromeos::CreateUserWallpaperDelegate();
}

void ChromeShellDelegate::HandleMediaNextTrack() {
  extensions::MediaPlayerAPI::Get(
      ProfileManager::GetDefaultProfileOrOffTheRecord())->
          media_player_event_router()->NotifyNextTrack();
}

void ChromeShellDelegate::HandleMediaPlayPause() {
  extensions::MediaPlayerAPI::Get(
      ProfileManager::GetDefaultProfileOrOffTheRecord())->
          media_player_event_router()->NotifyTogglePlayState();
}

void ChromeShellDelegate::HandleMediaPrevTrack() {
  extensions::MediaPlayerAPI::Get(
      ProfileManager::GetDefaultProfileOrOffTheRecord())->
          media_player_event_router()->NotifyPrevTrack();
}

void ChromeShellDelegate::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED:
      ash::Shell::GetInstance()->CreateLauncher();
      break;
    case chrome::NOTIFICATION_SESSION_STARTED:
      ash::Shell::GetInstance()->ShowLauncher();
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
