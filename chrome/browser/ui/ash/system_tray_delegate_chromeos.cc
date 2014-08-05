// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_delegate_chromeos.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/ime/input_method_menu_item.h"
#include "ash/ime/input_method_menu_manager.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/session/session_state_delegate.h"
#include "ash/session/session_state_observer.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/bluetooth/bluetooth_observer.h"
#include "ash/system/chromeos/session/logout_button_observer.h"
#include "ash/system/date/clock_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/user/login_status.h"
#include "ash/system/user/update_observer.h"
#include "ash/system/user/user_observer.h"
#include "ash/volume_control_delegate.h"
#include "ash/wm/lock_state_controller.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_pairing_dialog.h"
#include "chrome/browser/chromeos/charger_replace/charger_replacement_dialog.h"
#include "chrome/browser/chromeos/enrollment_dialog_view.h"
#include "chrome/browser/chromeos/events/system_key_event_listener.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/multiprofiles_intro_dialog.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/set_time_dialog.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/chromeos/ui/choose_mobile_network_dialog.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/user_accounts_delegate_chromeos.h"
#include "chrome/browser/ui/ash/volume_controller_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chromeos/charger_replacement_handler.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_dialog.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/ime_keyboard.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/google/core/browser/google_util.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "grit/ash_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace chromeos {

namespace {

// The minimum session length limit that can be set.
const int kSessionLengthLimitMinMs = 30 * 1000;  // 30 seconds.

// The maximum session length limit that can be set.
const int kSessionLengthLimitMaxMs = 24 * 60 * 60 * 1000;  // 24 hours.

const char kDisplaySettingsSubPageName[] = "display";
const char kDisplayOverscanSettingsSubPageName[] = "displayOverscan";

void ExtractIMEInfo(const input_method::InputMethodDescriptor& ime,
                    const input_method::InputMethodUtil& util,
                    ash::IMEInfo* info) {
  info->id = ime.id();
  info->name = util.GetInputMethodLongName(ime);
  info->medium_name = util.GetInputMethodMediumName(ime);
  info->short_name = util.GetInputMethodShortName(ime);
  info->third_party = extension_ime_util::IsExtensionIME(ime.id());
}

gfx::NativeWindow GetNativeWindowByStatus(ash::user::LoginStatus login_status,
                                          bool session_started) {
  int container_id =
      (!session_started || login_status == ash::user::LOGGED_IN_NONE ||
       login_status == ash::user::LOGGED_IN_LOCKED)
          ? ash::kShellWindowId_LockSystemModalContainer
          : ash::kShellWindowId_SystemModalContainer;
  return ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                  container_id);
}

void BluetoothPowerFailure() {
  // TODO(sad): Show an error bubble?
}

void BluetoothSetDiscoveringError() {
  LOG(ERROR) << "BluetoothSetDiscovering failed.";
}

void BluetoothDeviceConnectError(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  // TODO(sad): Do something?
}

void ShowSettingsSubPageForActiveUser(const std::string& sub_page) {
  chrome::ShowSettingsSubPageForProfile(
      ProfileManager::GetActiveUserProfile(), sub_page);
}

void ShowNetworkSettingsPage(const std::string& service_path) {
  std::string page = chrome::kInternetOptionsSubPage;
  page += "?servicePath=" + net::EscapeUrlEncodedData(service_path, true);
  content::RecordAction(base::UserMetricsAction("OpenInternetOptionsDialog"));
  ShowSettingsSubPageForActiveUser(page);
}

void OnAcceptMultiprofilesIntro(bool no_show_again) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kMultiProfileNeverShowIntro, no_show_again);
  UserAddingScreen::Get()->Start();
}

}  // namespace

SystemTrayDelegateChromeOS::SystemTrayDelegateChromeOS()
    : weak_ptr_factory_(this),
      user_profile_(NULL),
      clock_type_(base::GetHourClockType()),
      search_key_mapped_to_(input_method::kSearchKey),
      screen_locked_(false),
      have_session_start_time_(false),
      have_session_length_limit_(false),
      should_run_bluetooth_discovery_(false),
      volume_control_delegate_(new VolumeController()),
      device_settings_observer_(CrosSettings::Get()->AddSettingsObserver(
          kSystemUse24HourClock,
          base::Bind(&SystemTrayDelegateChromeOS::UpdateClockType,
                     base::Unretained(this)))) {
  // Register notifications on construction so that events such as
  // PROFILE_CREATED do not get missed if they happen before Initialize().
  registrar_.reset(new content::NotificationRegistrar);
  registrar_->Add(this,
                  chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                  content::NotificationService::AllSources());
  registrar_->Add(this,
                  chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                  content::NotificationService::AllSources());
  if (GetUserLoginStatus() == ash::user::LOGGED_IN_NONE) {
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
  DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(this);

  input_method::InputMethodManager::Get()->AddObserver(this);
  ash::ime::InputMethodMenuManager::GetInstance()->AddObserver(this);
  UpdateClockType();

  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&SystemTrayDelegateChromeOS::InitializeOnAdapterReady,
                 weak_ptr_factory_.GetWeakPtr()));

  ash::Shell::GetInstance()->session_state_delegate()->AddSessionStateObserver(
      this);

  if (LoginState::IsInitialized())
    LoginState::Get()->AddObserver(this);

  if (CrasAudioHandler::IsInitialized())
    CrasAudioHandler::Get()->AddAudioObserver(this);

  BrowserList::AddObserver(this);
}

void SystemTrayDelegateChromeOS::Shutdown() {
  device_settings_observer_.reset();
}

void SystemTrayDelegateChromeOS::InitializeOnAdapterReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  CHECK(bluetooth_adapter_.get());
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
  ash::ime::InputMethodMenuManager::GetInstance()->RemoveObserver(this);
  bluetooth_adapter_->RemoveObserver(this);
  ash::Shell::GetInstance()
      ->session_state_delegate()
      ->RemoveSessionStateObserver(this);
  LoginState::Get()->RemoveObserver(this);

  if (CrasAudioHandler::IsInitialized())
    CrasAudioHandler::Get()->RemoveAudioObserver(this);

  BrowserList::RemoveObserver(this);
  StopObservingAppWindowRegistry();

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  if (policy_manager)
    policy_manager->core()->store()->RemoveObserver(this);
}

// Overridden from ash::SystemTrayDelegate:
bool SystemTrayDelegateChromeOS::GetTrayVisibilityOnStartup() {
  // In case of OOBE / sign in screen tray will be shown later.
  return LoginState::Get()->IsUserLoggedIn();
}

ash::user::LoginStatus SystemTrayDelegateChromeOS::GetUserLoginStatus() const {
  // All non-logged in ChromeOS specific LOGGED_IN states map to the same
  // Ash specific LOGGED_IN state.
  if (!LoginState::Get()->IsUserLoggedIn())
    return ash::user::LOGGED_IN_NONE;

  if (screen_locked_)
    return ash::user::LOGGED_IN_LOCKED;

  LoginState::LoggedInUserType user_type =
      LoginState::Get()->GetLoggedInUserType();
  switch (user_type) {
    case LoginState::LOGGED_IN_USER_NONE:
      return ash::user::LOGGED_IN_NONE;
    case LoginState::LOGGED_IN_USER_REGULAR:
      return ash::user::LOGGED_IN_USER;
    case LoginState::LOGGED_IN_USER_OWNER:
      return ash::user::LOGGED_IN_OWNER;
    case LoginState::LOGGED_IN_USER_GUEST:
      return ash::user::LOGGED_IN_GUEST;
    case LoginState::LOGGED_IN_USER_RETAIL_MODE:
      return ash::user::LOGGED_IN_RETAIL_MODE;
    case LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT:
      return ash::user::LOGGED_IN_PUBLIC;
    case LoginState::LOGGED_IN_USER_SUPERVISED:
      return ash::user::LOGGED_IN_SUPERVISED;
    case LoginState::LOGGED_IN_USER_KIOSK_APP:
      return ash::user::LOGGED_IN_KIOSK_APP;
  }
  NOTREACHED();
  return ash::user::LOGGED_IN_NONE;
}

void SystemTrayDelegateChromeOS::ChangeProfilePicture() {
  content::RecordAction(
      base::UserMetricsAction("OpenChangeProfilePictureDialog"));
  ShowSettingsSubPageForActiveUser(chrome::kChangeProfilePictureSubPage);
}

const std::string SystemTrayDelegateChromeOS::GetEnterpriseDomain() const {
  return enterprise_domain_;
}

const base::string16 SystemTrayDelegateChromeOS::GetEnterpriseMessage() const {
  if (GetEnterpriseDomain().empty())
    return base::string16();
  return l10n_util::GetStringFUTF16(IDS_DEVICE_OWNED_BY_NOTICE,
                                    base::UTF8ToUTF16(GetEnterpriseDomain()));
}

const std::string SystemTrayDelegateChromeOS::GetSupervisedUserManager() const {
  if (GetUserLoginStatus() != ash::user::LOGGED_IN_SUPERVISED)
    return std::string();
  return UserManager::Get()->GetSupervisedUserManager()->GetManagerDisplayEmail(
      chromeos::UserManager::Get()->GetActiveUser()->email());
}

const base::string16
SystemTrayDelegateChromeOS::GetSupervisedUserManagerName() const {
  if (GetUserLoginStatus() != ash::user::LOGGED_IN_SUPERVISED)
    return base::string16();
  return UserManager::Get()->GetSupervisedUserManager()->GetManagerDisplayName(
      chromeos::UserManager::Get()->GetActiveUser()->email());
}

const base::string16 SystemTrayDelegateChromeOS::GetSupervisedUserMessage()
    const {
  if (GetUserLoginStatus() != ash::user::LOGGED_IN_SUPERVISED)
    return base::string16();
  return l10n_util::GetStringFUTF16(
      IDS_USER_IS_SUPERVISED_BY_NOTICE,
      base::UTF8ToUTF16(GetSupervisedUserManager()));
}

bool SystemTrayDelegateChromeOS::SystemShouldUpgrade() const {
  return UpgradeDetector::GetInstance()->notify_upgrade();
}

base::HourClockType SystemTrayDelegateChromeOS::GetHourClockType() const {
  return clock_type_;
}

void SystemTrayDelegateChromeOS::ShowSettings() {
  ShowSettingsSubPageForActiveUser("");
}

bool SystemTrayDelegateChromeOS::ShouldShowSettings() {
  return UserManager::Get()->GetCurrentUserFlow()->ShouldShowSettings();
}

void SystemTrayDelegateChromeOS::ShowDateSettings() {
  content::RecordAction(base::UserMetricsAction("ShowDateOptions"));
  std::string sub_page =
      std::string(chrome::kSearchSubPage) + "#" +
      l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME);
  // Everybody can change the time zone (even though it is a device setting).
  ShowSettingsSubPageForActiveUser(sub_page);
}

void SystemTrayDelegateChromeOS::ShowSetTimeDialog() {
  SetTimeDialog::ShowDialog(GetNativeWindow());
}

void SystemTrayDelegateChromeOS::ShowNetworkSettings(
    const std::string& service_path) {
  if (!LoginState::Get()->IsUserLoggedIn())
    return;
  ShowNetworkSettingsPage(service_path);
}

void SystemTrayDelegateChromeOS::ShowBluetoothSettings() {
  // TODO(sad): Make this work.
}

void SystemTrayDelegateChromeOS::ShowDisplaySettings() {
  content::RecordAction(base::UserMetricsAction("ShowDisplayOptions"));
  ShowSettingsSubPageForActiveUser(kDisplaySettingsSubPageName);
}

void SystemTrayDelegateChromeOS::ShowChromeSlow() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetPrimaryUserProfile(), chrome::HOST_DESKTOP_TYPE_ASH);
  chrome::ShowSlow(displayer.browser());
}

bool SystemTrayDelegateChromeOS::ShouldShowDisplayNotification() {
  // Packaged app is not counted as 'last active', so if a browser opening the
  // display settings is in background of a packaged app, it will return true.
  // TODO(mukai): fix this.
  Browser* active_browser =
      chrome::FindLastActiveWithHostDesktopType(chrome::HOST_DESKTOP_TYPE_ASH);
  if (!active_browser)
    return true;

  content::WebContents* active_contents =
      active_browser->tab_strip_model()->GetActiveWebContents();
  if (!active_contents)
    return true;

  GURL visible_url = active_contents->GetLastCommittedURL();
  GURL display_settings_url =
      chrome::GetSettingsUrl(kDisplaySettingsSubPageName);
  GURL display_overscan_url =
      chrome::GetSettingsUrl(kDisplayOverscanSettingsSubPageName);
  return (visible_url != display_settings_url &&
          visible_url != display_overscan_url);
}

void SystemTrayDelegateChromeOS::ShowIMESettings() {
  content::RecordAction(base::UserMetricsAction("OpenLanguageOptionsDialog"));
  ShowSettingsSubPageForActiveUser(chrome::kLanguageOptionsSubPage);
}

void SystemTrayDelegateChromeOS::ShowHelp() {
  chrome::ShowHelpForProfile(ProfileManager::GetActiveUserProfile(),
                             chrome::HOST_DESKTOP_TYPE_ASH,
                             chrome::HELP_SOURCE_MENU);
}

void SystemTrayDelegateChromeOS::ShowAccessibilityHelp() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile(), chrome::HOST_DESKTOP_TYPE_ASH);
  accessibility::ShowAccessibilityHelp(displayer.browser());
}

void SystemTrayDelegateChromeOS::ShowAccessibilitySettings() {
  content::RecordAction(base::UserMetricsAction("ShowAccessibilitySettings"));
  std::string sub_page = std::string(chrome::kSearchSubPage) + "#" +
                         l10n_util::GetStringUTF8(
                             IDS_OPTIONS_SETTINGS_SECTION_TITLE_ACCESSIBILITY);
  ShowSettingsSubPageForActiveUser(sub_page);
}

void SystemTrayDelegateChromeOS::ShowPublicAccountInfo() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile(), chrome::HOST_DESKTOP_TYPE_ASH);
  chrome::ShowPolicy(displayer.browser());
}

void SystemTrayDelegateChromeOS::ShowSupervisedUserInfo() {
  // TODO(antrim): find out what should we show in this case.
  // http://crbug.com/229762
}

void SystemTrayDelegateChromeOS::ShowEnterpriseInfo() {
  ash::user::LoginStatus status = GetUserLoginStatus();
  if (status == ash::user::LOGGED_IN_NONE ||
      status == ash::user::LOGGED_IN_LOCKED) {
    scoped_refptr<chromeos::HelpAppLauncher> help_app(
        new chromeos::HelpAppLauncher(GetNativeWindow()));
    help_app->ShowHelpTopic(chromeos::HelpAppLauncher::HELP_ENTERPRISE);
  } else {
    chrome::ScopedTabbedBrowserDisplayer displayer(
        ProfileManager::GetActiveUserProfile(), chrome::HOST_DESKTOP_TYPE_ASH);
    chrome::ShowSingletonTab(displayer.browser(),
                             GURL(chrome::kLearnMoreEnterpriseURL));
  }
}

void SystemTrayDelegateChromeOS::ShowUserLogin() {
  ash::Shell* shell = ash::Shell::GetInstance();
  if (!shell->delegate()->IsMultiProfilesEnabled())
    return;

  // Only regular users could add other users to current session.
  if (UserManager::Get()->GetActiveUser()->GetType() !=
      user_manager::USER_TYPE_REGULAR) {
    return;
  }

  if (static_cast<int>(UserManager::Get()->GetLoggedInUsers().size()) >=
      shell->session_state_delegate()->GetMaximumNumberOfLoggedInUsers())
    return;

  // Launch sign in screen to add another user to current session.
  if (UserManager::Get()->GetUsersAdmittedForMultiProfile().size()) {
    // Don't show dialog if any logged in user in multi-profiles session
    // dismissed it.
    bool show_intro = true;
    const user_manager::UserList logged_in_users =
        UserManager::Get()->GetLoggedInUsers();
    for (user_manager::UserList::const_iterator it = logged_in_users.begin();
         it != logged_in_users.end();
         ++it) {
      show_intro &= !multi_user_util::GetProfileFromUserID(
                         multi_user_util::GetUserIDFromEmail((*it)->email()))
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

bool SystemTrayDelegateChromeOS::ShowSpringChargerReplacementDialog() {
  if (!ChargerReplacementDialog::ShouldShowDialog())
    return false;

  ChargerReplacementDialog* dialog =
      new ChargerReplacementDialog(GetNativeWindow());
  dialog->Show();
  return true;
}

bool SystemTrayDelegateChromeOS::IsSpringChargerReplacementDialogVisible() {
  return ChargerReplacementDialog::IsDialogVisible();
}

bool SystemTrayDelegateChromeOS::HasUserConfirmedSafeSpringCharger() {
  return ChargerReplacementHandler::GetChargerStatusPref() ==
         ChargerReplacementHandler::CONFIRM_SAFE_CHARGER;
}

void SystemTrayDelegateChromeOS::ShutDown() {
  ash::Shell::GetInstance()->lock_state_controller()->RequestShutdown();
}

void SystemTrayDelegateChromeOS::SignOut() {
  chrome::AttemptUserExit();
}

void SystemTrayDelegateChromeOS::RequestLockScreen() {
  // TODO(antrim) : additional logging for crbug/173178
  LOG(WARNING) << "Requesting screen lock from AshSystemTrayDelegate";
  DBusThreadManager::Get()->GetSessionManagerClient()->RequestLockScreen();
}

void SystemTrayDelegateChromeOS::RequestRestartForUpdate() {
  // We expect that UpdateEngine is in "Reboot for update" state now.
  chrome::NotifyAndTerminate(true /* fast path */);
}

void SystemTrayDelegateChromeOS::GetAvailableBluetoothDevices(
    ash::BluetoothDeviceList* list) {
  device::BluetoothAdapter::DeviceList devices =
      bluetooth_adapter_->GetDevices();
  for (size_t i = 0; i < devices.size(); ++i) {
    device::BluetoothDevice* device = devices[i];
    ash::BluetoothDeviceInfo info;
    info.address = device->GetAddress();
    info.display_name = device->GetName();
    info.connected = device->IsConnected();
    info.connecting = device->IsConnecting();
    info.paired = device->IsPaired();
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
    ash::Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        ash::UMA_STATUS_AREA_BLUETOOTH_CONNECT_KNOWN_DEVICE);
    device->Connect(NULL,
                    base::Bind(&base::DoNothing),
                    base::Bind(&BluetoothDeviceConnectError));
  } else {  // Show paring dialog for the unpaired device.
    ash::Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        ash::UMA_STATUS_AREA_BLUETOOTH_CONNECT_UNKNOWN_DEVICE);
    BluetoothPairingDialog* dialog =
        new BluetoothPairingDialog(GetNativeWindow(), device);
    // The dialog deletes itself on close.
    dialog->Show();
  }
}

bool SystemTrayDelegateChromeOS::IsBluetoothDiscovering() {
  return bluetooth_adapter_->IsDiscovering();
}

void SystemTrayDelegateChromeOS::GetCurrentIME(ash::IMEInfo* info) {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
  input_method::InputMethodDescriptor ime = manager->GetCurrentInputMethod();
  ExtractIMEInfo(ime, *util, info);
  info->selected = true;
}

void SystemTrayDelegateChromeOS::GetAvailableIMEList(ash::IMEInfoList* list) {
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
  scoped_ptr<input_method::InputMethodDescriptors> ime_descriptors(
      manager->GetActiveInputMethods());
  std::string current = manager->GetCurrentInputMethod().id();
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
  ash::ime::InputMethodMenuItemList menu_list =
      ash::ime::InputMethodMenuManager::GetInstance()->
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
  input_method::InputMethodManager::Get()->ChangeInputMethod(ime_id);
}

void SystemTrayDelegateChromeOS::ActivateIMEProperty(const std::string& key) {
  input_method::InputMethodManager::Get()->ActivateInputMethodMenuItem(key);
}

void SystemTrayDelegateChromeOS::ShowNetworkConfigure(
    const std::string& network_id,
    gfx::NativeWindow parent_window) {
  NetworkConfigView::Show(network_id, parent_window);
}

bool SystemTrayDelegateChromeOS::EnrollNetwork(
    const std::string& network_id,
    gfx::NativeWindow parent_window) {
  return enrollment::CreateDialog(network_id, parent_window);
}

void SystemTrayDelegateChromeOS::ManageBluetoothDevices() {
  content::RecordAction(base::UserMetricsAction("ShowBluetoothSettingsPage"));
  std::string sub_page =
      std::string(chrome::kSearchSubPage) + "#" +
      l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_SECTION_TITLE_BLUETOOTH);
  ShowSettingsSubPageForActiveUser(sub_page);
}

void SystemTrayDelegateChromeOS::ToggleBluetooth() {
  bluetooth_adapter_->SetPowered(!bluetooth_adapter_->IsPowered(),
                                 base::Bind(&base::DoNothing),
                                 base::Bind(&BluetoothPowerFailure));
}

void SystemTrayDelegateChromeOS::ShowMobileSimDialog() {
  SimDialogDelegate::ShowDialog(GetNativeWindow(),
                                SimDialogDelegate::SIM_DIALOG_UNLOCK);
}

void SystemTrayDelegateChromeOS::ShowMobileSetupDialog(
    const std::string& service_path) {
  MobileSetupDialog::Show(service_path);
}

void SystemTrayDelegateChromeOS::ShowOtherNetworkDialog(
    const std::string& type) {
  if (type == shill::kTypeCellular) {
    ChooseMobileNetworkDialog::ShowDialog(GetNativeWindow());
    return;
  }
  NetworkConfigView::ShowForType(type, GetNativeWindow());
}

bool SystemTrayDelegateChromeOS::GetBluetoothAvailable() {
  return bluetooth_adapter_->IsPresent();
}

bool SystemTrayDelegateChromeOS::GetBluetoothEnabled() {
  return bluetooth_adapter_->IsPowered();
}

bool SystemTrayDelegateChromeOS::GetBluetoothDiscovering() {
  return (bluetooth_discovery_session_.get() &&
      bluetooth_discovery_session_->IsActive());
}

void SystemTrayDelegateChromeOS::ChangeProxySettings() {
  CHECK(GetUserLoginStatus() == ash::user::LOGGED_IN_NONE);
  LoginDisplayHostImpl::default_host()->OpenProxySettings();
}

ash::VolumeControlDelegate*
SystemTrayDelegateChromeOS::GetVolumeControlDelegate() const {
  return volume_control_delegate_.get();
}

void SystemTrayDelegateChromeOS::SetVolumeControlDelegate(
    scoped_ptr<ash::VolumeControlDelegate> delegate) {
  volume_control_delegate_.swap(delegate);
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
  GetSystemTrayNotifier()->NotifyUserUpdate();
}

bool SystemTrayDelegateChromeOS::IsSearchKeyMappedToCapsLock() {
  return search_key_mapped_to_ == input_method::kCapsLockKey;
}

ash::tray::UserAccountsDelegate*
SystemTrayDelegateChromeOS::GetUserAccountsDelegate(
    const std::string& user_id) {
  if (!accounts_delegates_.contains(user_id)) {
    const user_manager::User* user = UserManager::Get()->FindUser(user_id);
    Profile* user_profile = ProfileHelper::Get()->GetProfileByUser(user);
    CHECK(user_profile);
    accounts_delegates_.set(
        user_id,
        scoped_ptr<ash::tray::UserAccountsDelegate>(
            new UserAccountsDelegateChromeOS(user_profile)));
  }
  return accounts_delegates_.get(user_id);
}

ash::SystemTray* SystemTrayDelegateChromeOS::GetPrimarySystemTray() {
  return ash::Shell::GetInstance()->GetPrimarySystemTray();
}

ash::SystemTrayNotifier* SystemTrayDelegateChromeOS::GetSystemTrayNotifier() {
  return ash::Shell::GetInstance()->system_tray_notifier();
}

void SystemTrayDelegateChromeOS::SetProfile(Profile* profile) {
  // Stop observing the AppWindowRegistry of the current |user_profile_|.
  StopObservingAppWindowRegistry();

  user_profile_ = profile;

  // Start observing the AppWindowRegistry of the newly set |user_profile_|.
  apps::AppWindowRegistry::Get(user_profile_)->AddObserver(this);

  PrefService* prefs = profile->GetPrefs();
  user_pref_registrar_.reset(new PrefChangeRegistrar);
  user_pref_registrar_->Init(prefs);
  user_pref_registrar_->Add(
      prefs::kUse24HourClock,
      base::Bind(&SystemTrayDelegateChromeOS::UpdateClockType,
                 base::Unretained(this)));
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
                 base::Unretained(this),
                 ash::A11Y_NOTIFICATION_NONE));
  user_pref_registrar_->Add(
      prefs::kAccessibilityAutoclickEnabled,
      base::Bind(&SystemTrayDelegateChromeOS::OnAccessibilityModeChanged,
                 base::Unretained(this),
                 ash::A11Y_NOTIFICATION_NONE));
  user_pref_registrar_->Add(
      prefs::kShouldAlwaysShowAccessibilityMenu,
      base::Bind(&SystemTrayDelegateChromeOS::OnAccessibilityModeChanged,
                 base::Unretained(this),
                 ash::A11Y_NOTIFICATION_NONE));
  user_pref_registrar_->Add(
      prefs::kPerformanceTracingEnabled,
      base::Bind(&SystemTrayDelegateChromeOS::UpdatePerformanceTracing,
                 base::Unretained(this)));

  UpdateClockType();
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

bool SystemTrayDelegateChromeOS::GetShouldUse24HourClockForTesting() const {
  return ShouldUse24HourClock();
}

bool SystemTrayDelegateChromeOS::ShouldUse24HourClock() const {
  // On login screen and in guest mode owner default is used for
  // kUse24HourClock preference.
  const ash::user::LoginStatus status = GetUserLoginStatus();
  const CrosSettings* const cros_settings = CrosSettings::Get();
  bool system_use_24_hour_clock = true;
  const bool system_value_found = cros_settings->GetBoolean(
      kSystemUse24HourClock, &system_use_24_hour_clock);

  if ((status == ash::user::LOGGED_IN_NONE) || !user_pref_registrar_)
    return (system_value_found
                ? system_use_24_hour_clock
                : (base::GetHourClockType() == base::k24HourClock));

  const PrefService::Preference* user_pref =
      user_pref_registrar_->prefs()->FindPreference(prefs::kUse24HourClock);
  if (status == ash::user::LOGGED_IN_GUEST && user_pref->IsDefaultValue())
    return (system_value_found
                ? system_use_24_hour_clock
                : (base::GetHourClockType() == base::k24HourClock));

  bool use_24_hour_clock = true;
  user_pref->GetValue()->GetAsBoolean(&use_24_hour_clock);
  return use_24_hour_clock;
}

void SystemTrayDelegateChromeOS::UpdateClockType() {
  const bool use_24_hour_clock = ShouldUse24HourClock();
  clock_type_ = use_24_hour_clock ? base::k24HourClock : base::k12HourClock;
  GetSystemTrayNotifier()->NotifyDateFormatChanged();
  // This also works for enterprise-managed devices because they never have
  // local owner.
  if (chromeos::UserManager::Get()->IsCurrentUserOwner())
    CrosSettings::Get()->SetBoolean(kSystemUse24HourClock, use_24_hour_clock);
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

  apps::AppWindowRegistry* registry =
      apps::AppWindowRegistry::Factory::GetForBrowserContext(user_profile_,
                                                             false);
  if (registry)
    registry->RemoveObserver(this);
}

void SystemTrayDelegateChromeOS::NotifyIfLastWindowClosed() {
  if (!user_profile_)
    return;

  BrowserList* browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  for (BrowserList::const_iterator it = browser_list->begin();
       it != browser_list->end();
       ++it) {
    if ((*it)->profile()->IsSameProfile(user_profile_)) {
      // The current user has at least one open browser window.
      return;
    }
  }

  if (!apps::AppWindowRegistry::Get(user_profile_)->app_windows().empty()) {
    // The current user has at least one open app window.
    return;
  }

  GetSystemTrayNotifier()->NotifyLastWindowClosed();
}

// LoginState::Observer overrides.
void SystemTrayDelegateChromeOS::LoggedInStateChanged() {
  // It apparently sometimes takes a while after login before the current user
  // is recognized as the owner. Make sure that the system-wide clock setting
  // is updated when the recognition eventually happens
  // (http://crbug.com/278601).
  //
  // Note that it isn't safe to blindly call UpdateClockType() from this
  // method, as LoggedInStateChanged() is also called before the logged-in
  // user's profile has actually been loaded (http://crbug.com/317745). The
  // system tray's time format is updated at login via SetProfile().
  if (chromeos::UserManager::Get()->IsCurrentUserOwner()) {
    CrosSettings::Get()->SetBoolean(kSystemUse24HourClock,
                                    ShouldUse24HourClock());
  }
}

// Overridden from SessionManagerClient::Observer.
void SystemTrayDelegateChromeOS::ScreenIsLocked() {
  screen_locked_ = true;
  ash::Shell::GetInstance()->UpdateAfterLoginStatusChange(GetUserLoginStatus());
}

void SystemTrayDelegateChromeOS::ScreenIsUnlocked() {
  screen_locked_ = false;
  ash::Shell::GetInstance()->UpdateAfterLoginStatusChange(GetUserLoginStatus());
}

gfx::NativeWindow SystemTrayDelegateChromeOS::GetNativeWindow() const {
  bool session_started = ash::Shell::GetInstance()
                             ->session_state_delegate()
                             ->IsActiveUserSessionStarted();
  return GetNativeWindowByStatus(GetUserLoginStatus(), session_started);
}

// content::NotificationObserver implementation.
void SystemTrayDelegateChromeOS::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_UPGRADE_RECOMMENDED: {
      UpgradeDetector* detector =
          content::Source<UpgradeDetector>(source).ptr();
      ash::UpdateObserver::UpdateSeverity severity =
          ash::UpdateObserver::UPDATE_NORMAL;
      switch (detector->upgrade_notification_stage()) {
        case UpgradeDetector::UPGRADE_ANNOYANCE_SEVERE:
          severity = ash::UpdateObserver::UPDATE_SEVERE_RED;
          break;

        case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
          severity = ash::UpdateObserver::UPDATE_HIGH_ORANGE;
          break;

        case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
          severity = ash::UpdateObserver::UPDATE_LOW_GREEN;
          break;

        case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
        default:
          severity = ash::UpdateObserver::UPDATE_NORMAL;
          break;
      }
      GetSystemTrayNotifier()->NotifyUpdateRecommended(severity);
      break;
    }
    case chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED: {
      // This notification is also sent on login screen when user avatar
      // is loaded from file.
      if (GetUserLoginStatus() != ash::user::LOGGED_IN_NONE) {
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
      ash::Shell::GetInstance()->UpdateAfterLoginStatusChange(
          GetUserLoginStatus());
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
    bool show_message) {
  GetSystemTrayNotifier()->NotifyRefreshIME();
}

// Overridden from InputMethodMenuManager::Observer.
void SystemTrayDelegateChromeOS::InputMethodMenuItemChanged(
    ash::ime::InputMethodMenuManager* manager) {
  GetSystemTrayNotifier()->NotifyRefreshIME();
}

// Overridden from CrasAudioHandler::AudioObserver.
void SystemTrayDelegateChromeOS::OnOutputVolumeChanged() {
  GetSystemTrayNotifier()->NotifyAudioOutputVolumeChanged();
}

void SystemTrayDelegateChromeOS::OnOutputMuteChanged() {
  GetSystemTrayNotifier()->NotifyAudioOutputMuteChanged();
}

void SystemTrayDelegateChromeOS::OnInputGainChanged() {
}

void SystemTrayDelegateChromeOS::OnInputMuteChanged() {
}

void SystemTrayDelegateChromeOS::OnAudioNodesChanged() {
  GetSystemTrayNotifier()->NotifyAudioNodesChanged();
}

void SystemTrayDelegateChromeOS::OnActiveOutputNodeChanged() {
  GetSystemTrayNotifier()->NotifyAudioActiveOutputNodeChanged();
}

void SystemTrayDelegateChromeOS::OnActiveInputNodeChanged() {
  GetSystemTrayNotifier()->NotifyAudioActiveInputNodeChanged();
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
    scoped_ptr<device::BluetoothDiscoverySession> discovery_session) {
  // If the discovery session was returned after a request to stop discovery
  // (e.g. the user dismissed the Bluetooth detailed view before the call
  // returned), don't claim the discovery session and let it clean up.
  if (!should_run_bluetooth_discovery_)
    return;
  VLOG(1) << "Claiming new Bluetooth device discovery session.";
  bluetooth_discovery_session_ = discovery_session.Pass();
  GetSystemTrayNotifier()->NotifyBluetoothDiscoveringChanged();
}

void SystemTrayDelegateChromeOS::UpdateEnterpriseDomain() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  std::string enterprise_domain = connector->GetEnterpriseDomain();
  if (enterprise_domain_ != enterprise_domain) {
    enterprise_domain_ = enterprise_domain;
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
    const std::string& user_id) {
  GetSystemTrayNotifier()->NotifyUserAddedToSession();
}

// Overridden from chrome::BrowserListObserver.
void SystemTrayDelegateChromeOS::OnBrowserRemoved(Browser* browser) {
  NotifyIfLastWindowClosed();
}

// Overridden from apps::AppWindowRegistry::Observer.
void SystemTrayDelegateChromeOS::OnAppWindowRemoved(
    apps::AppWindow* app_window) {
  NotifyIfLastWindowClosed();
}

void SystemTrayDelegateChromeOS::OnAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  if (details.notification_type == ACCESSIBILITY_MANAGER_SHUTDOWN)
    accessibility_subscription_.reset();
  else
    OnAccessibilityModeChanged(details.notify);
}

ash::SystemTrayDelegate* CreateSystemTrayDelegate() {
  return new SystemTrayDelegateChromeOS();
}

}  // namespace chromeos
