// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include <stddef.h>

#include "ash/container_delegate_aura.h"
#include "ash/content/gpu_support_impl.h"
#include "ash/pointer_watcher_delegate_aura.h"
#include "ash/session/session_state_delegate.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/window_util.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/ash/app_list/app_list_service_ash.h"
#include "chrome/browser/ui/ash/chrome_keyboard_ui.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/session_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/grit/chromium_strings.h"
#include "grit/theme_resources.h"
#include "ui/app_list/presenter/app_list_presenter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/display/display_configuration_observer.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

namespace {

const char kKeyboardShortcutHelpPageUrl[] =
    "https://support.google.com/chromebook/answer/183101";

}  // namespace

ChromeShellDelegate::ChromeShellDelegate()
    : shelf_delegate_(NULL) {
  PlatformInit();
}

ChromeShellDelegate::~ChromeShellDelegate() {
}

bool ChromeShellDelegate::IsMultiProfilesEnabled() const {
  if (!profiles::IsMultipleProfilesEnabled())
    return false;
#if defined(OS_CHROMEOS)
  // If there is a user manager, we need to see that we can at least have 2
  // simultaneous users to allow this feature.
  if (!user_manager::UserManager::IsInitialized())
    return false;
  size_t admitted_users_to_be_added =
      user_manager::UserManager::Get()->GetUsersAllowedForMultiProfile().size();
  size_t logged_in_users =
      user_manager::UserManager::Get()->GetLoggedInUsers().size();
  if (!logged_in_users) {
    // The shelf gets created on the login screen and as such we have to create
    // all multi profile items of the the system tray menu before the user logs
    // in. For special cases like Kiosk mode and / or guest mode this isn't a
    // problem since either the browser gets restarted and / or the flag is not
    // allowed, but for an "ephermal" user (see crbug.com/312324) it is not
    // decided yet if he could add other users to his session or not.
    // TODO(skuhne): As soon as the issue above needs to be resolved, this logic
    // should change.
    logged_in_users = 1;
  }
  if (admitted_users_to_be_added + logged_in_users <= 1)
    return false;
#endif
  return true;
}

bool ChromeShellDelegate::IsIncognitoAllowed() const {
#if defined(OS_CHROMEOS)
  return chromeos::AccessibilityManager::Get()->IsIncognitoAllowed();
#endif
  return true;
}

bool ChromeShellDelegate::IsRunningInForcedAppMode() const {
  return chrome::IsRunningInForcedAppMode();
}

bool ChromeShellDelegate::CanShowWindowForUser(aura::Window* window) const {
  return ::CanShowWindowForUser(window, base::Bind(&GetActiveBrowserContext));
}

bool ChromeShellDelegate::IsForceMaximizeOnFirstRun() const {
#if defined(OS_CHROMEOS)
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (user) {
    return chromeos::ProfileHelper::Get()
        ->GetProfileByUser(user)
        ->GetPrefs()
        ->GetBoolean(prefs::kForceMaximizeOnFirstRun);
  }
#endif
  return false;
}

void ChromeShellDelegate::Exit() {
  chrome::AttemptUserExit();
}

void ChromeShellDelegate::OpenUrl(const GURL& url) {
  if (!url.is_valid())
    return;

  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::AddSelectedTabWithURL(displayer.browser(), url,
                                ui::PAGE_TRANSITION_LINK);

  // Since the ScopedTabbedBrowserDisplayer does not guarantee that the
  // browser will be shown on the active desktop, we ensure the visibility.
  multi_user_util::MoveWindowToCurrentDesktop(
      displayer.browser()->window()->GetNativeWindow());
}

app_list::AppListPresenter* ChromeShellDelegate::GetAppListPresenter() {
  DCHECK(ash::Shell::HasInstance());
  return AppListServiceAsh::GetInstance()->GetAppListPresenter();
}

ash::ShelfDelegate* ChromeShellDelegate::CreateShelfDelegate(
    ash::ShelfModel* model) {
  if (!shelf_delegate_) {
    shelf_delegate_ = ChromeLauncherController::CreateInstance(NULL, model);
    shelf_delegate_->Init();
  }
  return shelf_delegate_;
}

std::unique_ptr<ash::ContainerDelegate>
ChromeShellDelegate::CreateContainerDelegate() {
  return base::WrapUnique(new ash::ContainerDelegateAura);
}

std::unique_ptr<ash::PointerWatcherDelegate>
ChromeShellDelegate::CreatePointerWatcherDelegate() {
  return base::WrapUnique(new ash::PointerWatcherDelegateAura);
}

ui::MenuModel* ChromeShellDelegate::CreateContextMenu(
    ash::Shelf* shelf,
    const ash::ShelfItem* item) {
  DCHECK(shelf_delegate_);
  // Don't show context menu for exclusive app runtime mode.
  if (chrome::IsRunningInAppMode())
    return nullptr;

  return LauncherContextMenu::Create(shelf_delegate_, item, shelf);
}

ash::GPUSupport* ChromeShellDelegate::CreateGPUSupport() {
  // Chrome uses real GPU support.
  return new ash::GPUSupportImpl;
}

base::string16 ChromeShellDelegate::GetProductName() const {
  return l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
}

void ChromeShellDelegate::OpenKeyboardShortcutHelpPage() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  Browser* browser = chrome::FindTabbedBrowser(profile, false);

  if (!browser) {
    browser = new Browser(Browser::CreateParams(profile));
    browser->window()->Show();
  }

  browser->window()->Activate();

  chrome::NavigateParams params(browser, GURL(kKeyboardShortcutHelpPageUrl),
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = SINGLETON_TAB;
  chrome::Navigate(&params);
}

gfx::Image ChromeShellDelegate::GetDeprecatedAcceleratorImage() const {
  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_BLUETOOTH_KEYBOARD);
}

void ChromeShellDelegate::ToggleTouchpad() {
#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->ToggleTouchpad();
#endif  // defined(OS_CHROMEOS)
}

void ChromeShellDelegate::ToggleTouchscreen() {
#if defined(OS_CHROMEOS)
  chromeos::system::InputDeviceSettings::Get()->ToggleTouchscreen();
#endif  // defined(OS_CHROMEOS)
}

keyboard::KeyboardUI* ChromeShellDelegate::CreateKeyboardUI() {
  return new ChromeKeyboardUI(ProfileManager::GetActiveUserProfile());
}

void ChromeShellDelegate::VirtualKeyboardActivated(bool activated) {
  FOR_EACH_OBSERVER(ash::VirtualKeyboardStateObserver,
                    keyboard_state_observer_list_,
                    OnVirtualKeyboardStateChanged(activated));
}

void ChromeShellDelegate::AddVirtualKeyboardStateObserver(
    ash::VirtualKeyboardStateObserver* observer) {
  keyboard_state_observer_list_.AddObserver(observer);
}

void ChromeShellDelegate::RemoveVirtualKeyboardStateObserver(
    ash::VirtualKeyboardStateObserver* observer) {
  keyboard_state_observer_list_.RemoveObserver(observer);
}
