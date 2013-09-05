// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "apps/native_app_window.h"
#include "apps/shell_window_registry.h"
#include "ash/keyboard_overlay/keyboard_overlay_view.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/background/ash_user_wallpaper_delegate.h"
#include "chrome/browser/chromeos/display/display_preferences.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/chromeos/extensions/media_player_event_router.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/system/ash_system_tray_delegate.h"
#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/ui/ash/caps_lock_delegate_chromeos.h"
#include "chrome/browser/ui/ash/session_state_delegate_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/ime/input_method_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

namespace {

// This function is used for restoring focus after the user session is started.
// It's needed because some windows can be opened in background while login UI
// is still active because we currently restore browser windows before login UI
// is deleted.
void RestoreFocus() {
  ash::MruWindowTracker::WindowList mru_list =
      ash::Shell::GetInstance()->mru_window_tracker()->BuildMruWindowList();
  if (!mru_list.empty())
    mru_list.front()->Focus();
}

}  // anonymous namespace

bool ChromeShellDelegate::IsFirstRunAfterBoot() const {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kFirstExecAfterBoot);
}

void ChromeShellDelegate::PreInit() {
  chromeos::LoadDisplayPreferences(IsFirstRunAfterBoot());
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
    using file_manager::kFileManagerAppId;
    Profile* const profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    const apps::ShellWindowRegistry* const registry =
        apps::ShellWindowRegistry::Get(profile);
    const apps::ShellWindowRegistry::ShellWindowList list =
        registry->GetShellWindowsForApp(kFileManagerAppId);
    if (list.empty()) {
      // Open the new window.
      const ExtensionService* const service = profile->GetExtensionService();
      if (service == NULL ||
          !service->IsExtensionEnabledForLauncher(kFileManagerAppId))
        return;
      const extensions::Extension* const extension =
          service->GetInstalledExtension(kFileManagerAppId);
      // event_flags = 0 means this invokes the same behavior as the launcher
      // item is clicked without any keyboard modifiers.
      chrome::OpenApplication(
          chrome::AppLaunchParams(profile, extension, 0 /* event_flags */));
    } else {
      // Activate the existing window.
      list.front()->GetBaseWindow()->Activate();
    }
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
  page->GetView()->Focus();
}

void ChromeShellDelegate::ToggleHighContrast() {
  DCHECK(chromeos::AccessibilityManager::Get());
  bool enabled = chromeos::AccessibilityManager::Get()->IsHighContrastEnabled();
  chromeos::AccessibilityManager::Get()->EnableHighContrast(!enabled);
}

bool ChromeShellDelegate::IsSpokenFeedbackEnabled() const {
  DCHECK(chromeos::AccessibilityManager::Get());
  return chromeos::AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
}

void ChromeShellDelegate::ToggleSpokenFeedback(
    ash::AccessibilityNotificationVisibility notify) {
  DCHECK(chromeos::AccessibilityManager::Get());
  chromeos::AccessibilityManager::Get()->ToggleSpokenFeedback(notify);
}

bool ChromeShellDelegate::IsHighContrastEnabled() const {
  DCHECK(chromeos::AccessibilityManager::Get());
  return chromeos::AccessibilityManager::Get()->IsHighContrastEnabled();
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

void ChromeShellDelegate::SetLargeCursorEnabled(bool enabled) {
  DCHECK(chromeos::AccessibilityManager::Get());
  return chromeos::AccessibilityManager::Get()->EnableLargeCursor(enabled);
}

bool ChromeShellDelegate::IsLargeCursorEnabled() const {
  DCHECK(chromeos::AccessibilityManager::Get());
  return chromeos::AccessibilityManager::Get()->IsLargeCursorEnabled();
}

ash::CapsLockDelegate* ChromeShellDelegate::CreateCapsLockDelegate() {
  chromeos::input_method::XKeyboard* xkeyboard =
      chromeos::input_method::InputMethodManager::Get()->GetXKeyboard();
  return new CapsLockDelegate(xkeyboard);
}

ash::SessionStateDelegate* ChromeShellDelegate::CreateSessionStateDelegate() {
  return new SessionStateDelegateChromeos;
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
  Profile* profile = ProfileManager::GetDefaultProfile();
  if (!profile)
    return false;

  PrefService* user_pref_service = profile->GetPrefs();
  return user_pref_service &&
      user_pref_service->GetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu);
}

void ChromeShellDelegate::SilenceSpokenFeedback() const {
  TtsController::GetInstance()->Stop();
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
      RestoreFocus();
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
