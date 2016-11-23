// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_delegate_chromeos.h"

#include <stddef.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/common/login_status.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/session/session_state_observer.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/system/chromeos/bluetooth/bluetooth_observer.h"
#include "ash/common/system/chromeos/power/power_status.h"
#include "ash/common/system/chromeos/session/logout_button_observer.h"
#include "ash/common/system/date/clock_observer.h"
#include "ash/common/system/ime/ime_observer.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray_accessibility.h"
#include "ash/common/system/update/update_observer.h"
#include "ash/common/system/user/user_observer.h"
#include "ash/common/wm_shell.h"
#include "ash/system/chromeos/rotation/tray_rotation_lock.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_pairing_dialog.h"
#include "chrome/browser/chromeos/events/system_key_event_listener.h"
#include "chrome/browser/chromeos/input_method/input_method_switch_recorder.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/multiprofiles_intro_dialog.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/cast_config_delegate_media_router.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/networking_config_delegate_chromeos.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/ash/system_tray_delegate_utils.h"
#include "chrome/browser/ui/ash/vpn_delegate_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/google/core/browser/google_util.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/chromeos/ime/input_method_menu_item.h"
#include "ui/chromeos/ime/input_method_menu_manager.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

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

void BluetoothSetDiscoveringError() {
  LOG(ERROR) << "BluetoothSetDiscovering failed.";
}

void BluetoothDeviceConnectError(
    device::BluetoothDevice::ConnectErrorCode error_code) {
}

void OnAcceptMultiprofilesIntro(bool no_show_again) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kMultiProfileNeverShowIntro, no_show_again);
  UserAddingScreen::Get()->Start();
}

bool IsSessionInSecondaryLoginScreen() {
  return ash::WmShell::Get()
      ->GetSessionStateDelegate()
      ->IsInSecondaryLoginScreen();
}

}  // namespace

SystemTrayDelegateChromeOS::SystemTrayDelegateChromeOS()
    : cast_config_delegate_(base::MakeUnique<CastConfigDelegateMediaRouter>()),
      networking_config_delegate_(new NetworkingConfigDelegateChromeos()),
      vpn_delegate_(new VPNDelegateChromeOS),
      weak_ptr_factory_(this) {
  // Register notifications on construction so that events such as
  // PROFILE_CREATED do not get missed if they happen before Initialize().
  registrar_.reset(new content::NotificationRegistrar);
  registrar_->Add(this,
                  chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                  content::NotificationService::AllSources());
  registrar_->Add(this,
                  chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                  content::NotificationService::AllSources());
  if (GetUserLoginStatus() == ash::LoginStatus::NOT_LOGGED_IN) {
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

  user_manager::UserManager::Get()->AddSessionStateObserver(this);
}

void SystemTrayDelegateChromeOS::Initialize() {
  DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(this);

  input_method::InputMethodManager::Get()->AddObserver(this);
  input_method::InputMethodManager::Get()->AddImeMenuObserver(this);
  ui::ime::InputMethodMenuManager::GetInstance()->AddObserver(this);

  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&SystemTrayDelegateChromeOS::InitializeOnAdapterReady,
                 weak_ptr_factory_.GetWeakPtr()));

  ash::WmShell::Get()->GetSessionStateDelegate()->AddSessionStateObserver(this);

  BrowserList::AddObserver(this);
}

void SystemTrayDelegateChromeOS::InitializeOnAdapterReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  CHECK(bluetooth_adapter_);
  bluetooth_adapter_->AddObserver(this);

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

  policy::BrowserPolicyConnectorChromeOS* policy_connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      policy_connector->GetDeviceCloudPolicyManager();
  if (policy_manager)
    policy_manager->core()->store()->AddObserver(this);
  UpdateEnterpriseDomain();
}

SystemTrayDelegateChromeOS::~SystemTrayDelegateChromeOS() {
  // Unregister PrefChangeRegistrars.
  local_state_registrar_.reset();
  user_pref_registrar_.reset();

  // Unregister content notifications before destroying any components.
  registrar_.reset();

  // Unregister a11y status subscription.
  accessibility_subscription_.reset();

  DBusThreadManager::Get()->GetSessionManagerClient()->RemoveObserver(this);
  input_method::InputMethodManager::Get()->RemoveObserver(this);
  ui::ime::InputMethodMenuManager::GetInstance()->RemoveObserver(this);
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
  ash::WmShell::Get()->GetSessionStateDelegate()->RemoveSessionStateObserver(
      this);

  BrowserList::RemoveObserver(this);
  StopObservingAppWindowRegistry();
  StopObservingCustodianInfoChanges();

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  if (policy_manager)
    policy_manager->core()->store()->RemoveObserver(this);

  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
}

ash::LoginStatus SystemTrayDelegateChromeOS::GetUserLoginStatus() const {
  return SystemTrayClient::GetUserLoginStatus();
}

std::string SystemTrayDelegateChromeOS::GetEnterpriseDomain() const {
  return enterprise_domain_;
}

std::string SystemTrayDelegateChromeOS::GetEnterpriseRealm() const {
  return enterprise_realm_;
}

base::string16 SystemTrayDelegateChromeOS::GetEnterpriseMessage() const {
  if (!GetEnterpriseRealm().empty()) {
    // TODO(rsorokin): Maybe change a message for the Active Directory devices.
    return l10n_util::GetStringFUTF16(IDS_DEVICE_OWNED_BY_NOTICE,
                                      base::UTF8ToUTF16(GetEnterpriseRealm()));
  }
  if (!GetEnterpriseDomain().empty()) {
    return l10n_util::GetStringFUTF16(IDS_DEVICE_OWNED_BY_NOTICE,
                                      base::UTF8ToUTF16(GetEnterpriseDomain()));
  }
  return base::string16();
}

std::string SystemTrayDelegateChromeOS::GetSupervisedUserManager() const {
  if (!IsUserSupervised())
    return std::string();
  return SupervisedUserServiceFactory::GetForProfile(user_profile_)->
      GetCustodianEmailAddress();
}

base::string16
SystemTrayDelegateChromeOS::GetSupervisedUserManagerName() const {
  if (!IsUserSupervised())
    return base::string16();
  return base::UTF8ToUTF16(SupervisedUserServiceFactory::GetForProfile(
      user_profile_)->GetCustodianName());
}

base::string16 SystemTrayDelegateChromeOS::GetSupervisedUserMessage()
    const {
  if (!IsUserSupervised())
    return base::string16();
  if (IsUserChild())
    return GetChildUserMessage();
  return GetLegacySupervisedUserMessage();
}

bool SystemTrayDelegateChromeOS::IsUserSupervised() const {
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  return user && user->IsSupervised();
}

bool SystemTrayDelegateChromeOS::IsUserChild() const {
  return user_manager::UserManager::Get()->IsLoggedInAsChildUser();
}

void SystemTrayDelegateChromeOS::GetSystemUpdateInfo(
    ash::UpdateInfo* info) const {
  GetUpdateInfo(UpgradeDetector::GetInstance(), info);
}

bool SystemTrayDelegateChromeOS::ShouldShowSettings() const {
  // Show setting button only when the user flow allows and it's not in the
  // multi-profile login screen.
  return ChromeUserManager::Get()->GetCurrentUserFlow()->ShouldShowSettings() &&
         !IsSessionInSecondaryLoginScreen();
}

bool SystemTrayDelegateChromeOS::ShouldShowNotificationTray() const {
  // Show notification tray only when the user flow allows and it's not in the
  // multi-profile login screen.
  return ChromeUserManager::Get()
             ->GetCurrentUserFlow()
             ->ShouldShowNotificationTray() &&
         !IsSessionInSecondaryLoginScreen();
}

void SystemTrayDelegateChromeOS::ShowEnterpriseInfo() {
  // TODO(mash): Refactor out SessionStateDelegate and move to SystemTrayClient.
  ash::LoginStatus status = GetUserLoginStatus();
  if (status == ash::LoginStatus::NOT_LOGGED_IN ||
      status == ash::LoginStatus::LOCKED || IsSessionInSecondaryLoginScreen()) {
    scoped_refptr<chromeos::HelpAppLauncher> help_app(
        new chromeos::HelpAppLauncher(nullptr /* parent_window */));
    help_app->ShowHelpTopic(chromeos::HelpAppLauncher::HELP_ENTERPRISE);
  } else {
    chrome::ScopedTabbedBrowserDisplayer displayer(
        ProfileManager::GetActiveUserProfile());
    chrome::ShowSingletonTab(displayer.browser(),
                             GURL(chrome::kLearnMoreEnterpriseURL));
  }
}

void SystemTrayDelegateChromeOS::ShowUserLogin() {
  ash::WmShell* wm_shell = ash::WmShell::Get();
  if (!wm_shell->delegate()->IsMultiProfilesEnabled())
    return;

  // Only regular non-supervised users could add other users to current session.
  if (user_manager::UserManager::Get()->GetActiveUser()->GetType() !=
      user_manager::USER_TYPE_REGULAR) {
    return;
  }

  if (static_cast<int>(
          user_manager::UserManager::Get()->GetLoggedInUsers().size()) >=
      wm_shell->GetSessionStateDelegate()->GetMaximumNumberOfLoggedInUsers()) {
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

void SystemTrayDelegateChromeOS::GetAvailableBluetoothDevices(
    ash::BluetoothDeviceList* list) {
  device::BluetoothAdapter::DeviceList devices =
      bluetooth_adapter_->GetDevices();
  for (size_t i = 0; i < devices.size(); ++i) {
    device::BluetoothDevice* device = devices[i];
    ash::BluetoothDeviceInfo info;
    info.address = device->GetAddress();
    info.display_name = device->GetNameForDisplay();
    info.connected = device->IsConnected();
    info.connecting = device->IsConnecting();
    info.paired = device->IsPaired();
    info.device_type = device->GetDeviceType();
    list->push_back(info);
  }
}

void SystemTrayDelegateChromeOS::BluetoothStartDiscovering() {
  if (GetBluetoothDiscovering()) {
    LOG(WARNING) << "Already have active Bluetooth device discovery session.";
    return;
  }
  VLOG(1) << "Requesting new Bluetooth device discovery session.";
  should_run_bluetooth_discovery_ = true;
  bluetooth_adapter_->StartDiscoverySession(
      base::Bind(&SystemTrayDelegateChromeOS::OnStartBluetoothDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothSetDiscoveringError));
}

void SystemTrayDelegateChromeOS::BluetoothStopDiscovering() {
  should_run_bluetooth_discovery_ = false;
  if (!GetBluetoothDiscovering()) {
    LOG(WARNING) << "No active Bluetooth device discovery session.";
    return;
  }
  VLOG(1) << "Stopping Bluetooth device discovery session.";
  bluetooth_discovery_session_->Stop(
      base::Bind(&base::DoNothing), base::Bind(&BluetoothSetDiscoveringError));
}

void SystemTrayDelegateChromeOS::ConnectToBluetoothDevice(
    const std::string& address) {
  device::BluetoothDevice* device = bluetooth_adapter_->GetDevice(address);
  if (!device || device->IsConnecting() ||
      (device->IsConnected() && device->IsPaired())) {
    return;
  }
  if (device->IsPaired() && !device->IsConnectable())
    return;
  if (device->IsPaired() || !device->IsPairable()) {
    base::RecordAction(
        base::UserMetricsAction("StatusArea_Bluetooth_Connect_Known"));
    device->Connect(NULL,
                    base::Bind(&base::DoNothing),
                    base::Bind(&BluetoothDeviceConnectError));
    return;
  }
  // Show pairing dialog for the unpaired device.
  base::RecordAction(
      base::UserMetricsAction("StatusArea_Bluetooth_Connect_Unknown"));
  BluetoothPairingDialog* dialog = new BluetoothPairingDialog(device);
  // The dialog deletes itself on close.
  dialog->ShowInContainer(SystemTrayClient::GetDialogParentContainerId());
}

bool SystemTrayDelegateChromeOS::IsBluetoothDiscovering() const {
  return bluetooth_adapter_ && bluetooth_adapter_->IsDiscovering();
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

void SystemTrayDelegateChromeOS::ManageBluetoothDevices() {
  content::RecordAction(base::UserMetricsAction("ShowBluetoothSettingsPage"));
  chrome::ShowSettingsSubPageForProfile(ProfileManager::GetActiveUserProfile(),
                                        chrome::kBluetoothSubPage);
}

void SystemTrayDelegateChromeOS::ToggleBluetooth() {
  bluetooth_adapter_->SetPowered(!bluetooth_adapter_->IsPowered(),
                                 base::Bind(&base::DoNothing),
                                 base::Bind(&base::DoNothing));
}

bool SystemTrayDelegateChromeOS::GetBluetoothAvailable() {
  return bluetooth_adapter_ && bluetooth_adapter_->IsPresent();
}

bool SystemTrayDelegateChromeOS::GetBluetoothEnabled() {
  return bluetooth_adapter_ && bluetooth_adapter_->IsPowered();
}

bool SystemTrayDelegateChromeOS::GetBluetoothDiscovering() {
  return bluetooth_discovery_session_ &&
         bluetooth_discovery_session_->IsActive();
}

ash::CastConfigDelegate* SystemTrayDelegateChromeOS::GetCastConfigDelegate() {
  return cast_config_delegate_.get();
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

int SystemTrayDelegateChromeOS::GetSystemTrayMenuWidth() {
  return l10n_util::GetLocalizedContentsWidthInPixels(
      IDS_SYSTEM_TRAY_MENU_BUBBLE_WIDTH_PIXELS);
}

void SystemTrayDelegateChromeOS::ActiveUserWasChanged() {
  SetProfile(ProfileManager::GetActiveUserProfile());
  GetSystemTrayNotifier()->NotifyUserUpdate();
}

bool SystemTrayDelegateChromeOS::IsSearchKeyMappedToCapsLock() {
  return search_key_mapped_to_ == input_method::kCapsLockKey;
}

void SystemTrayDelegateChromeOS::AddCustodianInfoTrayObserver(
    ash::CustodianInfoTrayObserver* observer) {
  custodian_info_changed_observers_.AddObserver(observer);
}

void SystemTrayDelegateChromeOS::RemoveCustodianInfoTrayObserver(
    ash::CustodianInfoTrayObserver* observer) {
  custodian_info_changed_observers_.RemoveObserver(observer);
}

ash::VPNDelegate* SystemTrayDelegateChromeOS::GetVPNDelegate() const {
  return vpn_delegate_.get();
}

std::unique_ptr<ash::SystemTrayItem>
SystemTrayDelegateChromeOS::CreateRotationLockTrayItem(ash::SystemTray* tray) {
  return base::MakeUnique<ash::TrayRotationLock>(tray);
}

void SystemTrayDelegateChromeOS::UserAddedToSession(
    const user_manager::User* active_user) {
}

void SystemTrayDelegateChromeOS::ActiveUserChanged(
    const user_manager::User* /* active_user */) {
}

void SystemTrayDelegateChromeOS::UserChangedChildStatus(
    user_manager::User* user) {
  Profile* user_profile = ProfileHelper::Get()->GetProfileByUser(user);

  // Returned user_profile might be NULL on restoring Users on browser start.
  // At some point profile is not yet fully initiated.
  if (session_started_ && user_profile && user_profile_ == user_profile)
    ash::WmShell::Get()->UpdateAfterLoginStatusChange(GetUserLoginStatus());
}

ash::SystemTrayNotifier* SystemTrayDelegateChromeOS::GetSystemTrayNotifier() {
  return ash::WmShell::Get()->system_tray_notifier();
}

void SystemTrayDelegateChromeOS::SetProfile(Profile* profile) {
  // Stop observing the AppWindowRegistry of the current |user_profile_|.
  StopObservingAppWindowRegistry();

  // Stop observing custodian info changes of the current |user_profile_|.
  StopObservingCustodianInfoChanges();

  user_profile_ = profile;

  // Start observing the AppWindowRegistry of the newly set |user_profile_|.
  extensions::AppWindowRegistry::Get(user_profile_)->AddObserver(this);

  // Start observing custodian info changes of the newly set |user_profile_|.
  SupervisedUserServiceFactory::GetForProfile(profile)->AddObserver(this);

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
  OnCustodianInfoChanged();
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

void SystemTrayDelegateChromeOS::StopObservingCustodianInfoChanges() {
  if (!user_profile_)
    return;

  SupervisedUserService* service = SupervisedUserServiceFactory::GetForProfile(
      user_profile_);
  if (service)
    service->RemoveObserver(this);
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

// Overridden from SessionManagerClient::Observer.
void SystemTrayDelegateChromeOS::ScreenIsLocked() {
  ash::WmShell::Get()->UpdateAfterLoginStatusChange(GetUserLoginStatus());
}

void SystemTrayDelegateChromeOS::ScreenIsUnlocked() {
  ash::WmShell::Get()->UpdateAfterLoginStatusChange(GetUserLoginStatus());
}

// content::NotificationObserver implementation.
void SystemTrayDelegateChromeOS::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_UPGRADE_RECOMMENDED: {
      ash::UpdateInfo info;
      GetUpdateInfo(content::Source<UpgradeDetector>(source).ptr(), &info);
      GetSystemTrayNotifier()->NotifyUpdateRecommended(info);
      break;
    }
    case chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED: {
      // This notification is also sent on login screen when user avatar
      // is loaded from file.
      if (GetUserLoginStatus() != ash::LoginStatus::NOT_LOGGED_IN) {
        GetSystemTrayNotifier()->NotifyUserUpdate();
      }
      break;
    }
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
      ash::WmShell::Get()->UpdateAfterLoginStatusChange(GetUserLoginStatus());
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

// Overridden from BluetoothAdapter::Observer.
void SystemTrayDelegateChromeOS::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void SystemTrayDelegateChromeOS::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void SystemTrayDelegateChromeOS::AdapterDiscoveringChanged(
    device::BluetoothAdapter* adapter,
    bool discovering) {
  GetSystemTrayNotifier()->NotifyBluetoothDiscoveringChanged();
}

void SystemTrayDelegateChromeOS::DeviceAdded(device::BluetoothAdapter* adapter,
                                             device::BluetoothDevice* device) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void SystemTrayDelegateChromeOS::DeviceChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void SystemTrayDelegateChromeOS::DeviceRemoved(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void SystemTrayDelegateChromeOS::OnStartBluetoothDiscoverySession(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  // If the discovery session was returned after a request to stop discovery
  // (e.g. the user dismissed the Bluetooth detailed view before the call
  // returned), don't claim the discovery session and let it clean up.
  if (!should_run_bluetooth_discovery_)
    return;
  VLOG(1) << "Claiming new Bluetooth device discovery session.";
  bluetooth_discovery_session_ = std::move(discovery_session);
  GetSystemTrayNotifier()->NotifyBluetoothDiscoveringChanged();
}

void SystemTrayDelegateChromeOS::UpdateEnterpriseDomain() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  std::string enterprise_domain = connector->GetEnterpriseDomain();
  std::string enterprise_realm = connector->GetRealm();
  if (enterprise_domain_ != enterprise_domain ||
      enterprise_realm_ != enterprise_realm) {
    enterprise_domain_ = enterprise_domain;
    enterprise_realm_ = enterprise_realm;
    GetSystemTrayNotifier()->NotifyEnterpriseDomainChanged();
  }
}

// Overridden from CloudPolicyStore::Observer
void SystemTrayDelegateChromeOS::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  UpdateEnterpriseDomain();
}

void SystemTrayDelegateChromeOS::OnStoreError(policy::CloudPolicyStore* store) {
  UpdateEnterpriseDomain();
}

// Overridden from ash::SessionStateObserver
void SystemTrayDelegateChromeOS::UserAddedToSession(
    const AccountId& /*account_id*/) {
  GetSystemTrayNotifier()->NotifyUserAddedToSession();
}

void SystemTrayDelegateChromeOS::ActiveUserChanged(
    const AccountId& /* user_id */) {}

// Overridden from chrome::BrowserListObserver.
void SystemTrayDelegateChromeOS::OnBrowserRemoved(Browser* browser) {
  NotifyIfLastWindowClosed();
}

// Overridden from extensions::AppWindowRegistry::Observer.
void SystemTrayDelegateChromeOS::OnAppWindowRemoved(
    extensions::AppWindow* app_window) {
  NotifyIfLastWindowClosed();
}

// Overridden from SupervisedUserServiceObserver.
void SystemTrayDelegateChromeOS::OnCustodianInfoChanged() {
  for (ash::CustodianInfoTrayObserver& observer :
       custodian_info_changed_observers_) {
    observer.OnCustodianInfoChanged();
  }
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

const base::string16
SystemTrayDelegateChromeOS::GetLegacySupervisedUserMessage() const {
  std::string user_manager_name = GetSupervisedUserManager();
  return l10n_util::GetStringFUTF16(
      IDS_USER_IS_SUPERVISED_BY_NOTICE,
      base::UTF8ToUTF16(user_manager_name));
}

const base::string16
SystemTrayDelegateChromeOS::GetChildUserMessage() const {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(user_profile_);
  base::string16 first_custodian =
      base::UTF8ToUTF16(service->GetCustodianEmailAddress());
  base::string16 second_custodian =
      base::UTF8ToUTF16(service->GetSecondCustodianEmailAddress());
  LOG_IF(WARNING, first_custodian.empty()) <<
      "Returning incomplete child user message as manager not known yet.";
  if (second_custodian.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_CHILD_USER_IS_MANAGED_BY_ONE_PARENT_NOTICE, first_custodian);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_CHILD_USER_IS_MANAGED_BY_TWO_PARENTS_NOTICE,
        first_custodian,
        second_custodian);
  }
#endif

  LOG(WARNING) << "SystemTrayDelegateChromeOS::GetChildUserMessage call while "
               << "ENABLE_SUPERVISED_USERS undefined.";
  return base::string16();
}

ash::SystemTrayDelegate* CreateSystemTrayDelegate() {
  return new SystemTrayDelegateChromeOS();
}

}  // namespace chromeos
