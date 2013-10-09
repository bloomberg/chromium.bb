// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "apps/native_app_window.h"
#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "ash/host/root_window_host_factory.h"
#include "ash/magnifier/magnifier_constants.h"
#include "ash/session_state_delegate.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/ash/app_list/app_list_controller_ash.h"
#include "chrome/browser/ui/ash/ash_keyboard_controller_proxy.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/ash/user_action_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/default_pinned_apps_field_trial.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

// static
ChromeShellDelegate* ChromeShellDelegate::instance_ = NULL;

namespace {

void RestoreTabUsingProfile(Profile* profile) {
  TabRestoreService* service = TabRestoreServiceFactory::GetForProfile(profile);
  service->RestoreMostRecentEntry(NULL, chrome::HOST_DESKTOP_TYPE_ASH);
}

}  // namespace

// TabRestoreHelper is used to restore a tab. In particular when the user
// attempts to a restore a tab if the TabRestoreService hasn't finished loading
// this waits for it. Once the TabRestoreService finishes loading the tab is
// restored.
class ChromeShellDelegate::TabRestoreHelper : public TabRestoreServiceObserver {
 public:
  TabRestoreHelper(ChromeShellDelegate* delegate,
                   Profile* profile,
                   TabRestoreService* service)
      : delegate_(delegate),
        profile_(profile),
        tab_restore_service_(service) {
    tab_restore_service_->AddObserver(this);
  }

  virtual ~TabRestoreHelper() {
    tab_restore_service_->RemoveObserver(this);
  }

  TabRestoreService* tab_restore_service() { return tab_restore_service_; }

  virtual void TabRestoreServiceChanged(TabRestoreService* service) OVERRIDE {
  }
  virtual void TabRestoreServiceDestroyed(TabRestoreService* service) OVERRIDE {
    // This destroys us.
    delegate_->tab_restore_helper_.reset();
  }

  virtual void TabRestoreServiceLoaded(TabRestoreService* service) OVERRIDE {
    RestoreTabUsingProfile(profile_);
    // This destroys us.
    delegate_->tab_restore_helper_.reset();
  }

 private:
  ChromeShellDelegate* delegate_;
  Profile* profile_;
  TabRestoreService* tab_restore_service_;

  DISALLOW_COPY_AND_ASSIGN(TabRestoreHelper);
};

ChromeShellDelegate::ChromeShellDelegate()
    : launcher_delegate_(NULL) {
  instance_ = this;
  PlatformInit();
}

ChromeShellDelegate::~ChromeShellDelegate() {
  if (instance_ == this)
    instance_ = NULL;
}

bool ChromeShellDelegate::IsMultiProfilesEnabled() const {
  // TODO(skuhne): There is a function named profiles::IsMultiProfilesEnabled
  // which does similar things - but it is not the same. We should investigate
  // if these two could be folded together.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kMultiProfiles))
    return false;
#if defined(OS_CHROMEOS)
  // If there is a user manager, we need to see that we can at least have 2
  // simultaneous users to allow this feature.
  if (chromeos::UserManager::IsInitialized() &&
      chromeos::UserManager::Get()->GetUsersAdmittedForMultiProfile().size() +
      chromeos::UserManager::Get()->GetLoggedInUsers().size() <= 1)
    return false;
#endif
  return true;
}

bool ChromeShellDelegate::IsRunningInForcedAppMode() const {
  return chrome::IsRunningInForcedAppMode();
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

void ChromeShellDelegate::ToggleFullscreen() {
  // Only toggle if the user has a window open.
  aura::Window* window = ash::wm::GetActiveWindow();
  if (!window)
    return;
  ash::wm::WindowState* window_state = ash::wm::GetWindowState(window);

  bool is_fullscreen = window_state->IsFullscreen();

  // Windows which cannot be maximized should not be fullscreened.
  if (!is_fullscreen && !window_state->CanMaximize())
    return;

  Browser* browser = chrome::FindBrowserWithWindow(window);
  if (browser) {
    // If a window is fullscreen, exit fullscreen.
    if (is_fullscreen) {
      chrome::ToggleFullscreenMode(browser);
      return;
    }

    // AppNonClientFrameViewAsh shows only the window controls and no other
    // window decorations which is pretty close to fullscreen. Put v1 apps
    // into maximized mode instead of fullscreen to avoid showing the ugly
    // fullscreen exit bubble.
#if defined(OS_WIN)
    if (browser->host_desktop_type() == chrome::HOST_DESKTOP_TYPE_NATIVE) {
      chrome::ToggleFullscreenMode(browser);
      return;
    }
#endif  // OS_WIN
    if (browser->is_app() && browser->app_type() != Browser::APP_TYPE_CHILD)
      window_state->ToggleMaximized();
    else
      chrome::ToggleFullscreenMode(browser);
    return;
  }

  // |window| may belong to a shell window.
  apps::ShellWindow* shell_window = apps::ShellWindowRegistry::
      GetShellWindowForNativeWindowAnyProfile(window);
  if (shell_window) {
    if (is_fullscreen)
      shell_window->Restore();
    else
      shell_window->Fullscreen();
  }
}

void ChromeShellDelegate::ToggleMaximized() {
  // Only toggle if the user has a window open.
  aura::Window* window = ash::wm::GetActiveWindow();
  if (!window)
    return;

  ash::wm::WindowState* window_state = ash::wm::GetWindowState(window);
  // Get out of fullscreen when in fullscreen mode.
  if (window_state->IsFullscreen()) {
    ToggleFullscreen();
    return;
  }
  window_state->ToggleMaximized();
}

void ChromeShellDelegate::RestoreTab() {
  if (tab_restore_helper_.get()) {
    DCHECK(!tab_restore_helper_->tab_restore_service()->IsLoaded());
    return;
  }

  Browser* browser = chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
  Profile* profile = browser ? browser->profile() : NULL;
  if (!profile)
    profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  if (profile->IsOffTheRecord())
    return;
  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);
  if (!service)
    return;

  if (service->IsLoaded()) {
    RestoreTabUsingProfile(profile);
  } else {
    tab_restore_helper_.reset(new TabRestoreHelper(this, profile, service));
    service->LoadTabsFromLastSession();
  }
}

void ChromeShellDelegate::ShowTaskManager() {
  chrome::OpenTaskManager(NULL);
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
  DCHECK(ProfileManager::IsGetDefaultProfileAllowed());
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
  chrome::OpenFeedbackDialog(GetTargetBrowserIfAvailable());
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
    case ash::UMA_ACCEL_LOCK_SCREEN_L:
      content::RecordAction(
          content::UserMetricsAction("Accel_LockScreen_L"));
      break;
    case ash::UMA_ACCEL_LOCK_SCREEN_LOCK_BUTTON:
      content::RecordAction(
          content::UserMetricsAction("Accel_LockScreen_LockButton"));
      break;
    case ash::UMA_ACCEL_LOCK_SCREEN_POWER_BUTTON:
      content::RecordAction(
          content::UserMetricsAction("Accel_LockScreen_PowerButton"));
      break;
    case ash::UMA_ACCEL_FULLSCREEN_F4:
      content::RecordAction(content::UserMetricsAction("Accel_Fullscreen_F4"));
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
    case ash::UMA_ACCEL_OVERVIEW_F5:
      content::RecordAction(content::UserMetricsAction("Accel_Overview_F5"));
      break;
    case ash::UMA_ACCEL_PREVWINDOW_F5:
      content::RecordAction(content::UserMetricsAction("Accel_PrevWindow_F5"));
      break;
    case ash::UMA_ACCEL_PREVWINDOW_TAB:
      content::RecordAction(content::UserMetricsAction("Accel_PrevWindow_Tab"));
      break;
    case ash::UMA_ACCEL_EXIT_FIRST_Q:
      content::RecordAction(content::UserMetricsAction("Accel_Exit_First_Q"));
      break;
    case ash::UMA_ACCEL_EXIT_SECOND_Q:
      content::RecordAction(content::UserMetricsAction("Accel_Exit_Second_Q"));
      break;
    case ash::UMA_ACCEL_SEARCH_LWIN:
      content::RecordAction(content::UserMetricsAction("Accel_Search_LWin"));
      break;
    case ash::UMA_ACCEL_SHUT_DOWN_POWER_BUTTON:
      content::RecordAction(
          content::UserMetricsAction("Accel_ShutDown_PowerButton"));
      break;
    case ash::UMA_CLOSE_THROUGH_CONTEXT_MENU:
      content::RecordAction(content::UserMetricsAction("CloseFromContextMenu"));
      break;
    case ash::UMA_LAUNCHER_CLICK_ON_APP:
      content::RecordAction(content::UserMetricsAction("Launcher_ClickOnApp"));
      break;
    case ash::UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON:
      content::RecordAction(
          content::UserMetricsAction("Launcher_ClickOnApplistButton"));
#if defined(OS_CHROMEOS)
      chromeos::default_pinned_apps_field_trial::RecordShelfClick(
          chromeos::default_pinned_apps_field_trial::APP_LAUNCHER);
#endif
      break;
    case ash::UMA_MINIMIZE_PER_KEY:
      content::RecordAction(content::UserMetricsAction("Minimize_UsingKey"));
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
    case ash::UMA_TRAY_HELP:
      content::RecordAction(content::UserMetricsAction("Tray_Help"));
      break;
    case ash::UMA_TRAY_LOCK_SCREEN:
      content::RecordAction(content::UserMetricsAction("Tray_LockScreen"));
      break;
    case ash::UMA_TRAY_SHUT_DOWN:
      content::RecordAction(content::UserMetricsAction("Tray_ShutDown"));
      break;
    case ash::UMA_WINDOW_APP_CLOSE_BUTTON_CLICK:
      content::RecordAction(content::UserMetricsAction("AppCloseButton_Clk"));
      break;
    case ash::UMA_WINDOW_CLOSE_BUTTON_CLICK:
      content::RecordAction(content::UserMetricsAction("CloseButton_Clk"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_EXIT_FULLSCREEN:
      content::RecordAction(content::UserMetricsAction("MaxButton_Clk_ExitFS"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_RESTORE:
      content::RecordAction(
          content::UserMetricsAction("MaxButton_Clk_Restore"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MAXIMIZE:
      content::RecordAction(
          content::UserMetricsAction("MaxButton_Clk_Maximize"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MINIMIZE:
      content::RecordAction(content::UserMetricsAction("MinButton_Clk"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE:
      content::RecordAction(content::UserMetricsAction("MaxButton_Maximize"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_LEFT:
      content::RecordAction(content::UserMetricsAction("MaxButton_MaxLeft"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_RIGHT:
      content::RecordAction(content::UserMetricsAction("MaxButton_MaxRight"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_MINIMIZE:
      content::RecordAction(content::UserMetricsAction("MaxButton_Minimize"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_RESTORE:
      content::RecordAction(content::UserMetricsAction("MaxButton_Restore"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_SHOW_BUBBLE:
      content::RecordAction(content::UserMetricsAction("MaxButton_ShowBubble"));
      break;
    case ash::UMA_WINDOW_OVERVIEW:
      content::RecordAction(
          content::UserMetricsAction("WindowSelector_Overview"));
      break;
    case ash::UMA_WINDOW_SELECTION:
      content::RecordAction(
          content::UserMetricsAction("WindowSelector_Selection"));
      break;
  }
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
  Browser* browser = GetTargetBrowserIfAvailable();
  if (browser)
    return browser;
  return chrome::FindOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord(),
      chrome::HOST_DESKTOP_TYPE_ASH);
}

Browser* ChromeShellDelegate::GetTargetBrowserIfAvailable() {
  return chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
}

keyboard::KeyboardControllerProxy*
    ChromeShellDelegate::CreateKeyboardControllerProxy() {
  return new AshKeyboardControllerProxy();
}
