// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_delegate_chromeos.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/login_status.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/date/clock_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/power/power_status.h"
#include "ash/system/rotation/tray_rotation_lock.h"
#include "ash/system/session/logout_button_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray_accessibility.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/events/system_key_event_listener.h"
#include "chrome/browser/chromeos/input_method/input_method_switch_recorder.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/profiles/multiprofiles_intro_dialog.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/networking_config_delegate_chromeos.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/google/core/browser/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/chromeos/events/pref_names.h"
#include "ui/chromeos/ime/input_method_menu_item.h"
#include "ui/chromeos/ime/input_method_menu_manager.h"

namespace chromeos {

namespace {

// The minimum session length limit that can be set.
const int kSessionLengthLimitMinMs = 30 * 1000;  // 30 seconds.

// The maximum session length limit that can be set.
const int kSessionLengthLimitMaxMs = 24 * 60 * 60 * 1000;  // 24 hours.

void ExtractIMEInfo(const input_method::InputMethodDescriptor& ime,
                    const input_method::InputMethodUtil& util,
                    ash::IMEInfo* info) {
  info->id = ime.id();
  info->name = util.GetInputMethodLongName(ime);
  info->medium_name = util.GetInputMethodMediumName(ime);
  info->short_name = util.GetInputMethodShortName(ime);
  info->third_party = extension_ime_util::IsExtensionIME(ime.id());
}

void OnAcceptMultiprofilesIntro(bool no_show_again) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kMultiProfileNeverShowIntro, no_show_again);
  UserAddingScreen::Get()->Start();
}

}  // namespace

SystemTrayDelegateChromeOS::SystemTrayDelegateChromeOS()
    : networking_config_delegate_(
          base::MakeUnique<NetworkingConfigDelegateChromeos>()) {
  // Register notifications on construction so that events such as
  // PROFILE_CREATED do not get missed if they happen before Initialize().
  registrar_.reset(new content::NotificationRegistrar);
  if (SystemTrayClient::GetUserLoginStatus() ==
      ash::LoginStatus::NOT_LOGGED_IN) {
    registrar_->Add(this,
                    chrome::NOTIFICATION_SESSION_STARTED,
                    content::NotificationService::AllSources());
  }
  registrar_->Add(this,
                  chrome::NOTIFICATION_PROFILE_CREATED,
                  content::NotificationService::AllSources());
  registrar_->Add(this,
                  chrome::NOTIFICATION_PROFILE_DESTROYED,
                  content::NotificationService::AllSources());

  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  CHECK(accessibility_manager);
  accessibility_subscription_ = accessibility_manager->RegisterCallback(
      base::Bind(&SystemTrayDelegateChromeOS::OnAccessibilityStatusChanged,
                 base::Unretained(this)));
}

void SystemTrayDelegateChromeOS::Initialize() {
  input_method::InputMethodManager::Get()->AddObserver(this);
  input_method::InputMethodManager::Get()->AddImeMenuObserver(this);
  ui::ime::InputMethodMenuManager::GetInstance()->AddObserver(this);

  BrowserList::AddObserver(this);

  local_state_registrar_.reset(new PrefChangeRegistrar);
  local_state_registrar_->Init(g_browser_process->local_state());

  UpdateSessionStartTime();
  UpdateSessionLengthLimit();

  local_state_registrar_->Add(
      prefs::kSessionStartTime,
      base::Bind(&SystemTrayDelegateChromeOS::UpdateSessionStartTime,
                 base::Unretained(this)));
  local_state_registrar_->Add(
      prefs::kSessionLengthLimit,
      base::Bind(&SystemTrayDelegateChromeOS::UpdateSessionLengthLimit,
                 base::Unretained(this)));
}

SystemTrayDelegateChromeOS::~SystemTrayDelegateChromeOS() {
  // Unregister PrefChangeRegistrars.
  local_state_registrar_.reset();
  user_pref_registrar_.reset();

  // Unregister content notifications before destroying any components.
  registrar_.reset();

  // Unregister a11y status subscription.
  accessibility_subscription_.reset();

  input_method::InputMethodManager::Get()->RemoveObserver(this);
  ui::ime::InputMethodMenuManager::GetInstance()->RemoveObserver(this);

  BrowserList::RemoveObserver(this);
  StopObservingAppWindowRegistry();
}

void SystemTrayDelegateChromeOS::ShowUserLogin() {
  if (!ash::Shell::Get()->shell_delegate()->IsMultiProfilesEnabled())
    return;

  // Only regular non-supervised users could add other users to current session.
  if (user_manager::UserManager::Get()->GetActiveUser()->GetType() !=
      user_manager::USER_TYPE_REGULAR) {
    return;
  }

  if (user_manager::UserManager::Get()->GetLoggedInUsers().size() >=
      session_manager::kMaxmiumNumberOfUserSessions) {
    return;
  }

  // Launch sign in screen to add another user to current session.
  if (user_manager::UserManager::Get()
          ->GetUsersAllowedForMultiProfile()
          .size()) {
    // Don't show dialog if any logged in user in multi-profiles session
    // dismissed it.
    bool show_intro = true;
    const user_manager::UserList logged_in_users =
        user_manager::UserManager::Get()->GetLoggedInUsers();
    for (user_manager::UserList::const_iterator it = logged_in_users.begin();
         it != logged_in_users.end();
         ++it) {
      show_intro &=
          !multi_user_util::GetProfileFromAccountId((*it)->GetAccountId())
               ->GetPrefs()
               ->GetBoolean(prefs::kMultiProfileNeverShowIntro);
      if (!show_intro)
        break;
    }
    if (show_intro) {
      base::Callback<void(bool)> on_accept =
          base::Bind(&OnAcceptMultiprofilesIntro);
      ShowMultiprofilesIntroDialog(on_accept);
    } else {
      UserAddingScreen::Get()->Start();
    }
  }
}

void SystemTrayDelegateChromeOS::GetCurrentIME(ash::IMEInfo* info) {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
  input_method::InputMethodDescriptor ime =
      manager->GetActiveIMEState()->GetCurrentInputMethod();
  ExtractIMEInfo(ime, *util, info);
  info->selected = true;
}

void SystemTrayDelegateChromeOS::GetAvailableIMEList(ash::IMEInfoList* list) {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
  std::unique_ptr<input_method::InputMethodDescriptors> ime_descriptors(
      manager->GetActiveIMEState()->GetActiveInputMethods());
  std::string current =
      manager->GetActiveIMEState()->GetCurrentInputMethod().id();
  for (size_t i = 0; i < ime_descriptors->size(); i++) {
    input_method::InputMethodDescriptor& ime = ime_descriptors->at(i);
    ash::IMEInfo info;
    ExtractIMEInfo(ime, *util, &info);
    info.selected = ime.id() == current;
    list->push_back(info);
  }
}

void SystemTrayDelegateChromeOS::GetCurrentIMEProperties(
    ash::IMEPropertyInfoList* list) {
  ui::ime::InputMethodMenuItemList menu_list =
      ui::ime::InputMethodMenuManager::GetInstance()->
      GetCurrentInputMethodMenuItemList();
  for (size_t i = 0; i < menu_list.size(); ++i) {
    ash::IMEPropertyInfo property;
    property.key = menu_list[i].key;
    property.name = base::UTF8ToUTF16(menu_list[i].label);
    property.selected = menu_list[i].is_selection_item_checked;
    list->push_back(property);
  }
}

base::string16 SystemTrayDelegateChromeOS::GetIMEManagedMessage() {
  auto ime_state = input_method::InputMethodManager::Get()->GetActiveIMEState();
  return ime_state->GetAllowedInputMethods().empty()
             ? base::string16()
             : l10n_util::GetStringUTF16(IDS_OPTIONS_CONTROLLED_SETTING_POLICY);
}

void SystemTrayDelegateChromeOS::SwitchIME(const std::string& ime_id) {
  input_method::InputMethodManager::Get()
      ->GetActiveIMEState()
      ->ChangeInputMethod(ime_id, false /* show_message */);
  input_method::InputMethodSwitchRecorder::Get()->RecordSwitch(
      true /* by_tray_menu */);
}

void SystemTrayDelegateChromeOS::ActivateIMEProperty(const std::string& key) {
  input_method::InputMethodManager::Get()->ActivateInputMethodMenuItem(key);
}

ash::NetworkingConfigDelegate*
SystemTrayDelegateChromeOS::GetNetworkingConfigDelegate() const {
  return networking_config_delegate_.get();
}

bool SystemTrayDelegateChromeOS::GetSessionStartTime(
    base::TimeTicks* session_start_time) {
  *session_start_time = session_start_time_;
  return have_session_start_time_;
}

bool SystemTrayDelegateChromeOS::GetSessionLengthLimit(
    base::TimeDelta* session_length_limit) {
  *session_length_limit = session_length_limit_;
  return have_session_length_limit_;
}

void SystemTrayDelegateChromeOS::ActiveUserWasChanged() {
  SetProfile(ProfileManager::GetActiveUserProfile());
}

bool SystemTrayDelegateChromeOS::IsSearchKeyMappedToCapsLock() {
  return search_key_mapped_to_ == input_method::kCapsLockKey;
}

std::unique_ptr<ash::SystemTrayItem>
SystemTrayDelegateChromeOS::CreateRotationLockTrayItem(ash::SystemTray* tray) {
  return base::MakeUnique<ash::TrayRotationLock>(tray);
}

ash::SystemTrayNotifier* SystemTrayDelegateChromeOS::GetSystemTrayNotifier() {
  return ash::Shell::Get()->system_tray_notifier();
}

void SystemTrayDelegateChromeOS::SetProfile(Profile* profile) {
  // Stop observing the AppWindowRegistry of the current |user_profile_|.
  StopObservingAppWindowRegistry();

  user_profile_ = profile;

  // Start observing the AppWindowRegistry of the newly set |user_profile_|.
  extensions::AppWindowRegistry::Get(user_profile_)->AddObserver(this);

  PrefService* prefs = profile->GetPrefs();
  user_pref_registrar_.reset(new PrefChangeRegistrar);
  user_pref_registrar_->Init(prefs);
  user_pref_registrar_->Add(
      prefs::kLanguageRemapSearchKeyTo,
      base::Bind(&SystemTrayDelegateChromeOS::OnLanguageRemapSearchKeyToChanged,
                 base::Unretained(this)));
  user_pref_registrar_->Add(
      prefs::kShowLogoutButtonInTray,
      base::Bind(&SystemTrayDelegateChromeOS::UpdateShowLogoutButtonInTray,
                 base::Unretained(this)));
  user_pref_registrar_->Add(
      prefs::kLogoutDialogDurationMs,
      base::Bind(&SystemTrayDelegateChromeOS::UpdateLogoutDialogDuration,
                 base::Unretained(this)));
  user_pref_registrar_->Add(
      prefs::kAccessibilityLargeCursorEnabled,
      base::Bind(&SystemTrayDelegateChromeOS::OnAccessibilityModeChanged,
                 base::Unretained(this), ash::A11Y_NOTIFICATION_NONE));
  user_pref_registrar_->Add(
      prefs::kAccessibilityAutoclickEnabled,
      base::Bind(&SystemTrayDelegateChromeOS::OnAccessibilityModeChanged,
                 base::Unretained(this), ash::A11Y_NOTIFICATION_NONE));
  user_pref_registrar_->Add(
      prefs::kShouldAlwaysShowAccessibilityMenu,
      base::Bind(&SystemTrayDelegateChromeOS::OnAccessibilityModeChanged,
                 base::Unretained(this), ash::A11Y_NOTIFICATION_NONE));
  user_pref_registrar_->Add(
      prefs::kPerformanceTracingEnabled,
      base::Bind(&SystemTrayDelegateChromeOS::UpdatePerformanceTracing,
                 base::Unretained(this)));

  UpdateShowLogoutButtonInTray();
  UpdateLogoutDialogDuration();
  UpdatePerformanceTracing();
  search_key_mapped_to_ =
      profile->GetPrefs()->GetInteger(prefs::kLanguageRemapSearchKeyTo);
}

bool SystemTrayDelegateChromeOS::UnsetProfile(Profile* profile) {
  if (profile != user_profile_)
    return false;
  user_pref_registrar_.reset();
  user_profile_ = NULL;
  return true;
}

void SystemTrayDelegateChromeOS::UpdateShowLogoutButtonInTray() {
  GetSystemTrayNotifier()->NotifyShowLoginButtonChanged(
      user_pref_registrar_->prefs()->GetBoolean(
          prefs::kShowLogoutButtonInTray));
}

void SystemTrayDelegateChromeOS::UpdateLogoutDialogDuration() {
  const int duration_ms =
      user_pref_registrar_->prefs()->GetInteger(prefs::kLogoutDialogDurationMs);
  GetSystemTrayNotifier()->NotifyLogoutDialogDurationChanged(
      base::TimeDelta::FromMilliseconds(duration_ms));
}

void SystemTrayDelegateChromeOS::UpdateSessionStartTime() {
  const PrefService* local_state = local_state_registrar_->prefs();
  if (local_state->HasPrefPath(prefs::kSessionStartTime)) {
    have_session_start_time_ = true;
    session_start_time_ = base::TimeTicks::FromInternalValue(
        local_state->GetInt64(prefs::kSessionStartTime));
  } else {
    have_session_start_time_ = false;
    session_start_time_ = base::TimeTicks();
  }
  GetSystemTrayNotifier()->NotifySessionStartTimeChanged();
}

void SystemTrayDelegateChromeOS::UpdateSessionLengthLimit() {
  const PrefService* local_state = local_state_registrar_->prefs();
  if (local_state->HasPrefPath(prefs::kSessionLengthLimit)) {
    have_session_length_limit_ = true;
    session_length_limit_ = base::TimeDelta::FromMilliseconds(
        std::min(std::max(local_state->GetInteger(prefs::kSessionLengthLimit),
                          kSessionLengthLimitMinMs),
                 kSessionLengthLimitMaxMs));
  } else {
    have_session_length_limit_ = false;
    session_length_limit_ = base::TimeDelta();
  }
  GetSystemTrayNotifier()->NotifySessionLengthLimitChanged();
}

void SystemTrayDelegateChromeOS::StopObservingAppWindowRegistry() {
  if (!user_profile_)
    return;

  extensions::AppWindowRegistry* registry =
      extensions::AppWindowRegistry::Factory::GetForBrowserContext(
          user_profile_, false);
  if (registry)
    registry->RemoveObserver(this);
}

void SystemTrayDelegateChromeOS::NotifyIfLastWindowClosed() {
  if (!user_profile_)
    return;

  BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_iterator it = browser_list->begin();
       it != browser_list->end();
       ++it) {
    if ((*it)->profile()->IsSameProfile(user_profile_)) {
      // The current user has at least one open browser window.
      return;
    }
  }

  if (!extensions::AppWindowRegistry::Get(
          user_profile_)->app_windows().empty()) {
    // The current user has at least one open app window.
    return;
  }

  GetSystemTrayNotifier()->NotifyLastWindowClosed();
}

// content::NotificationObserver implementation.
void SystemTrayDelegateChromeOS::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      SetProfile(content::Source<Profile>(source).ptr());
      registrar_->Remove(this,
                         chrome::NOTIFICATION_PROFILE_CREATED,
                         content::NotificationService::AllSources());
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      if (UnsetProfile(content::Source<Profile>(source).ptr())) {
        registrar_->Remove(this,
                           chrome::NOTIFICATION_PROFILE_DESTROYED,
                           content::NotificationService::AllSources());
      }
      break;
    }
    case chrome::NOTIFICATION_SESSION_STARTED: {
      session_started_ = true;
      SetProfile(ProfileManager::GetActiveUserProfile());
      break;
    }
    default:
      NOTREACHED();
  }
}

void SystemTrayDelegateChromeOS::OnLanguageRemapSearchKeyToChanged() {
  search_key_mapped_to_ = user_pref_registrar_->prefs()->GetInteger(
      prefs::kLanguageRemapSearchKeyTo);
}

void SystemTrayDelegateChromeOS::OnAccessibilityModeChanged(
    ash::AccessibilityNotificationVisibility notify) {
  GetSystemTrayNotifier()->NotifyAccessibilityModeChanged(notify);
}

void SystemTrayDelegateChromeOS::UpdatePerformanceTracing() {
  if (!user_pref_registrar_)
    return;
  bool value = user_pref_registrar_->prefs()->GetBoolean(
      prefs::kPerformanceTracingEnabled);
  GetSystemTrayNotifier()->NotifyTracingModeChanged(value);
}

// Overridden from InputMethodManager::Observer.
void SystemTrayDelegateChromeOS::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* /* profile */,
    bool show_message) {
  GetSystemTrayNotifier()->NotifyRefreshIME();
}

// Overridden from InputMethodMenuManager::Observer.
void SystemTrayDelegateChromeOS::InputMethodMenuItemChanged(
    ui::ime::InputMethodMenuManager* manager) {
  GetSystemTrayNotifier()->NotifyRefreshIME();
}

// Overridden from chrome::BrowserListObserver.
void SystemTrayDelegateChromeOS::OnBrowserRemoved(Browser* browser) {
  NotifyIfLastWindowClosed();
}

// Overridden from extensions::AppWindowRegistry::Observer.
void SystemTrayDelegateChromeOS::OnAppWindowRemoved(
    extensions::AppWindow* app_window) {
  NotifyIfLastWindowClosed();
}

void SystemTrayDelegateChromeOS::OnAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  if (details.notification_type == ACCESSIBILITY_MANAGER_SHUTDOWN)
    accessibility_subscription_.reset();
  else
    OnAccessibilityModeChanged(details.notify);
}

void SystemTrayDelegateChromeOS::ImeMenuActivationChanged(bool is_active) {
  GetSystemTrayNotifier()->NotifyRefreshIMEMenu(is_active);
}

void SystemTrayDelegateChromeOS::ImeMenuListChanged() {}

void SystemTrayDelegateChromeOS::ImeMenuItemsChanged(
    const std::string& engine_id,
    const std::vector<input_method::InputMethodManager::MenuItem>& items) {}

ash::SystemTrayDelegate* CreateSystemTrayDelegate() {
  return new SystemTrayDelegateChromeOS();
}

}  // namespace chromeos
