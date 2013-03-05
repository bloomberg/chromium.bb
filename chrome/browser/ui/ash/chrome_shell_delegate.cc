// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "ash/ash_switches.h"
#include "ash/host/root_window_host_factory.h"
#include "ash/launcher/launcher_types.h"
#include "ash/magnifier/magnifier_constants.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/ash/app_list/app_list_controller_ash.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/ash/user_action_handler.h"
#include "chrome/browser/ui/ash/window_positioner.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/time_format.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/aura/client/user_action_client.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"

// static
ChromeShellDelegate* ChromeShellDelegate::instance_ = NULL;

ChromeShellDelegate::ChromeShellDelegate()
    : window_positioner_(new ash::WindowPositioner()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      launcher_delegate_(NULL) {
  instance_ = this;
  PlatformInit();
}

ChromeShellDelegate::~ChromeShellDelegate() {
  if (instance_ == this)
    instance_ = NULL;
}

bool ChromeShellDelegate::IsRunningInForcedAppMode() const {
  return chrome::IsRunningInForcedAppMode();
}

void ChromeShellDelegate::UnlockScreen() {
  // This is used only for testing thus far.
  NOTIMPLEMENTED();
}

void ChromeShellDelegate::Exit() {
  chrome::AttemptUserExit();
}

void ChromeShellDelegate::NewTab() {
  Browser* browser = GetTargetBrowser();
  // If the browser was not active, we call BrowserWindow::Show to make it
  // visible. Otherwise, we let Browser::NewTab handle the active window change.
  const bool was_active = browser->window()->IsActive();
  chrome::NewTab(browser);
  if (!was_active)
    browser->window()->Show();
}

void ChromeShellDelegate::NewWindow(bool is_incognito) {
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  chrome::NewEmptyWindow(
      is_incognito ? profile->GetOffTheRecordProfile() : profile,
      chrome::HOST_DESKTOP_TYPE_ASH);
}

void ChromeShellDelegate::ToggleMaximized() {
  // Only toggle if the user has a window open.
  aura::Window* window = ash::wm::GetActiveWindow();
  if (!window)
    return;

  // TODO(jamescook): If immersive mode replaces fullscreen, rename this
  // function and the interface to ToggleFullscreen.
  if (CommandLine::ForCurrentProcess()->
        HasSwitch(ash::switches::kAshImmersiveMode)) {
    chrome::ToggleFullscreenMode(GetTargetBrowser());
    return;
  }

  // Get out of fullscreen when in fullscreen mode.
  if (ash::wm::IsWindowFullscreen(window)) {
    chrome::ToggleFullscreenMode(GetTargetBrowser());
    return;
  }
  ash::wm::ToggleMaximizedWindow(window);
}

void ChromeShellDelegate::RestoreTab() {
  Browser* browser = GetTargetBrowser();
  // Do not restore tabs while in the incognito mode.
  if (browser->profile()->IsOffTheRecord())
    return;
  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser->profile());
  if (!service)
    return;
  if (service->IsLoaded()) {
    chrome::RestoreTab(browser);
  } else {
    service->LoadTabsFromLastSession();
    // LoadTabsFromLastSession is asynchronous, so TabRestoreService has not
    // finished loading the entries at this point. Wait for next event cycle
    // which loads the restored tab entries.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ChromeShellDelegate::RestoreTab,
                   weak_factory_.GetWeakPtr()));
  }
}

bool ChromeShellDelegate::RotatePaneFocus(ash::Shell::Direction direction) {
  aura::Window* window = ash::wm::GetActiveWindow();
  if (!window)
    return false;

  Browser* browser = chrome::FindBrowserWithWindow(window);
  if (!browser)
    return false;

  switch (direction) {
    case ash::Shell::FORWARD:
      chrome::FocusNextPane(browser);
      break;
    case ash::Shell::BACKWARD:
      chrome::FocusPreviousPane(browser);
      break;
  }
  return true;
}

void ChromeShellDelegate::ShowTaskManager() {
  Browser* browser = chrome::FindOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord(),
      chrome::HOST_DESKTOP_TYPE_ASH);
  chrome::OpenTaskManager(browser, false);
}

content::BrowserContext* ChromeShellDelegate::GetCurrentBrowserContext() {
  return ProfileManager::GetDefaultProfile();
}

app_list::AppListViewDelegate*
    ChromeShellDelegate::CreateAppListViewDelegate() {
  DCHECK(ash::Shell::HasInstance());
  // Shell will own the created delegate, and the delegate will own
  // the controller.
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  return new AppListViewDelegate(new AppListControllerDelegateAsh(), profile);
}

ash::LauncherDelegate* ChromeShellDelegate::CreateLauncherDelegate(
    ash::LauncherModel* model) {
  // TODO(oshima): This is currently broken with multiple launchers.
  // Refactor so that there is just one launcher delegate in the
  // shell.
  if (!launcher_delegate_) {
    launcher_delegate_ = ChromeLauncherController::CreateInstance(NULL, model);
    launcher_delegate_->Init();
  }
  return launcher_delegate_;
}

aura::client::UserActionClient* ChromeShellDelegate::CreateUserActionClient() {
  return new UserActionHandler;
}

void ChromeShellDelegate::OpenFeedbackPage() {
  chrome::OpenFeedbackDialog(GetTargetBrowser());
}

void ChromeShellDelegate::RecordUserMetricsAction(
    ash::UserMetricsAction action) {
  switch (action) {
    case ash::UMA_ACCEL_KEYBOARD_BRIGHTNESS_DOWN_F6:
      content::RecordAction(
          content::UserMetricsAction("Accel_KeyboardBrightnessDown_F6"));
      break;
    case ash::UMA_ACCEL_KEYBOARD_BRIGHTNESS_UP_F7:
      content::RecordAction(
          content::UserMetricsAction("Accel_KeyboardBrightnessUp_F7"));
      break;
    case ash::UMA_ACCEL_MAXIMIZE_RESTORE_F4:
      content::RecordAction(
          content::UserMetricsAction("Accel_Maximize_Restore_F4"));
      break;
    case ash::UMA_ACCEL_NEWTAB_T:
      content::RecordAction(content::UserMetricsAction("Accel_NewTab_T"));
      break;
    case ash::UMA_ACCEL_NEXTWINDOW_F5:
      content::RecordAction(content::UserMetricsAction("Accel_NextWindow_F5"));
      break;
    case ash::UMA_ACCEL_NEXTWINDOW_TAB:
      content::RecordAction(content::UserMetricsAction("Accel_NextWindow_Tab"));
      break;
    case ash::UMA_ACCEL_PREVWINDOW_F5:
      content::RecordAction(content::UserMetricsAction("Accel_PrevWindow_F5"));
      break;
    case ash::UMA_ACCEL_PREVWINDOW_TAB:
      content::RecordAction(content::UserMetricsAction("Accel_PrevWindow_Tab"));
      break;
    case ash::UMA_ACCEL_SEARCH_LWIN:
      content::RecordAction(content::UserMetricsAction("Accel_Search_LWin"));
      break;
    case ash::UMA_MAXIMIZE_BUTTON_MAXIMIZE:
      content::RecordAction(content::UserMetricsAction("MaxButton_Maximize"));
      break;
    case ash::UMA_MAXIMIZE_BUTTON_MAXIMIZE_LEFT:
      content::RecordAction(content::UserMetricsAction("MaxButton_MaxLeft"));
      break;
    case ash::UMA_MAXIMIZE_BUTTON_MAXIMIZE_RIGHT:
      content::RecordAction(content::UserMetricsAction("MaxButton_MaxRight"));
      break;
    case ash::UMA_MAXIMIZE_BUTTON_MINIMIZE:
      content::RecordAction(content::UserMetricsAction("MaxButton_Minimize"));
      break;
    case ash::UMA_MAXIMIZE_BUTTON_RESTORE:
      content::RecordAction(content::UserMetricsAction("MaxButton_Restore"));
      break;
    case ash::UMA_MAXIMIZE_BUTTON_SHOW_BUBBLE:
      content::RecordAction(content::UserMetricsAction("MaxButton_ShowBubble"));
      break;
    case ash::UMA_LAUNCHER_CLICK_ON_APP:
      content::RecordAction(content::UserMetricsAction("Launcher_ClickOnApp"));
      break;
    case ash::UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON:
      content::RecordAction(
          content::UserMetricsAction("Launcher_ClickOnApplistButton"));
      break;
    case ash::UMA_MOUSE_DOWN:
      content::RecordAction(content::UserMetricsAction("Mouse_Down"));
      break;
    case ash::UMA_TOGGLE_MAXIMIZE_CAPTION_CLICK:
      content::RecordAction(
          content::UserMetricsAction("Caption_ClickTogglesMaximize"));
      break;
    case ash::UMA_TOGGLE_MAXIMIZE_CAPTION_GESTURE:
      content::RecordAction(
          content::UserMetricsAction("Caption_GestureTogglesMaximize"));
      break;
    case ash::UMA_TOUCHSCREEN_TAP_DOWN:
      content::RecordAction(content::UserMetricsAction("Touchscreen_Down"));
      break;
  }
}

string16 ChromeShellDelegate::GetTimeRemainingString(base::TimeDelta delta) {
  return TimeFormat::TimeRemainingLong(delta);
}

string16 ChromeShellDelegate::GetTimeDurationLongString(base::TimeDelta delta) {
  return TimeFormat::TimeDurationLong(delta);
}

ui::MenuModel* ChromeShellDelegate::CreateContextMenu(aura::RootWindow* root) {
  DCHECK(launcher_delegate_);
  // Don't show context menu for exclusive app runtime mode.
  if (chrome::IsRunningInAppMode())
    return NULL;

  return new LauncherContextMenu(launcher_delegate_, root);
}

ash::RootWindowHostFactory* ChromeShellDelegate::CreateRootWindowHostFactory() {
  return ash::RootWindowHostFactory::Create();
}

string16 ChromeShellDelegate::GetProductName() const {
  return l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
}

Browser* ChromeShellDelegate::GetTargetBrowser() {
  Browser* browser = chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
  if (browser)
    return browser;
  return chrome::FindOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord(),
      chrome::HOST_DESKTOP_TYPE_ASH);
}
