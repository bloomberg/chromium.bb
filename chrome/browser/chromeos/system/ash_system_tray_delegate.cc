// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/ash_system_tray_delegate.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/session_state_delegate.h"
#include "ash/session_state_observer.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/bluetooth/bluetooth_observer.h"
#include "ash/system/date/clock_observer.h"
#include "ash/system/drive/drive_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/logout_button/logout_button_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/tray_caps_lock.h"
#include "ash/system/user/login_status.h"
#include "ash/system/user/update_observer.h"
#include "ash/system/user/user_observer.h"
#include "ash/volume_control_delegate.h"
#include "ash/wm/lock_state_controller.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
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
#include "chrome/browser/chromeos/choose_mobile_network_dialog.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/job_list.h"
#include "chrome/browser/chromeos/enrollment_dialog_view.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_adding_screen.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/chromeos/system_key_event_listener.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/feedback/tracing_manager.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/volume_controller_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_dialog.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/ime/xkeyboard.h"
#include "chromeos/login/login_state.h"
#include "content/public/browser/browser_thread.h"
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

using drive::DriveIntegrationService;
using drive::DriveIntegrationServiceFactory;

namespace chromeos {

namespace {

// The minimum session length limit that can be set.
const int kSessionLengthLimitMinMs = 30 * 1000;  // 30 seconds.

// The maximum session length limit that can be set.
const int kSessionLengthLimitMaxMs = 24 * 60 * 60 * 1000;  // 24 hours.

const char kDisplaySettingsSubPageName[] = "display";
const char kDisplayOverscanSettingsSubPageName[] = "displayOverscan";

// The URL for the Google Drive settings page.
const char kDriveSettingsPageURL[] = "https://drive.google.com";

void ExtractIMEInfo(const input_method::InputMethodDescriptor& ime,
                    const input_method::InputMethodUtil& util,
                    ash::IMEInfo* info) {
  info->id = ime.id();
  info->name = util.GetInputMethodLongName(ime);
  info->medium_name = util.GetInputMethodMediumName(ime);
  info->short_name = util.GetInputMethodShortName(ime);
  info->third_party = extension_ime_util::IsExtensionIME(ime.id());
}

gfx::NativeWindow GetNativeWindowByStatus(
    ash::user::LoginStatus login_status,
    bool session_started) {
  int container_id =
      (!session_started ||
       login_status == ash::user::LOGGED_IN_NONE ||
       login_status == ash::user::LOGGED_IN_LOCKED) ?
           ash::internal::kShellWindowId_LockSystemModalContainer :
           ash::internal::kShellWindowId_SystemModalContainer;
  return ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                  container_id);
}

// Converts drive::JobInfo to ash::DriveOperationStatus.
// If the job is not of type that ash tray is interested, returns false.
bool ConvertToDriveOperationStatus(const drive::JobInfo& info,
                                   ash::DriveOperationStatus* status) {
  if (info.job_type == drive::TYPE_DOWNLOAD_FILE) {
    status->type = ash::DriveOperationStatus::OPERATION_DOWNLOAD;
  } else if (info.job_type == drive::TYPE_UPLOAD_NEW_FILE ||
           info.job_type == drive::TYPE_UPLOAD_EXISTING_FILE) {
    status->type = ash::DriveOperationStatus::OPERATION_UPLOAD;
  } else {
    return false;
  }

  if (info.state == drive::STATE_NONE)
    status->state = ash::DriveOperationStatus::OPERATION_NOT_STARTED;
  else
    status->state = ash::DriveOperationStatus::OPERATION_IN_PROGRESS;

  status->id = info.job_id;
  status->file_path = info.file_path;
  status->progress = info.num_total_bytes == 0 ? 0.0 :
      static_cast<double>(info.num_completed_bytes) /
          static_cast<double>(info.num_total_bytes);
  return true;
}

// Converts drive::JobInfo that has finished in |error| state
// to ash::DriveOperationStatus.
// If the job is not of type that ash tray is interested, returns false.
bool ConvertToFinishedDriveOperationStatus(const drive::JobInfo& info,
                                           drive::FileError error,
                                           ash::DriveOperationStatus* status) {
  if (!ConvertToDriveOperationStatus(info, status))
    return false;
  status->state = (error == drive::FILE_ERROR_OK) ?
      ash::DriveOperationStatus::OPERATION_COMPLETED :
      ash::DriveOperationStatus::OPERATION_FAILED;
  return true;
}

// Converts a list of drive::JobInfo to a list of ash::DriveOperationStatusList.
ash::DriveOperationStatusList ConvertToDriveStatusList(
    const std::vector<drive::JobInfo>& list) {
  ash::DriveOperationStatusList results;
  for (size_t i = 0; i < list.size(); ++i) {
    ash::DriveOperationStatus status;
    if (ConvertToDriveOperationStatus(list[i], &status))
      results.push_back(status);
  }
  return results;
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

// Shows the settings sub page in the last active browser. If there is no such
// browser, creates a new browser with the settings sub page.
void ShowSettingsSubPageForAppropriateBrowser(const std::string& sub_page) {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetDefaultProfileOrOffTheRecord(),
      chrome::HOST_DESKTOP_TYPE_ASH);
  chrome::ShowSettingsSubPage(displayer.browser(), sub_page);
}

void ShowNetworkSettingsPage(const std::string& service_path) {
  std::string page = chrome::kInternetOptionsSubPage;
  page += "?servicePath=" + net::EscapeUrlEncodedData(service_path, true);
  content::RecordAction(
      content::UserMetricsAction("OpenInternetOptionsDialog"));
  ShowSettingsSubPageForAppropriateBrowser(page);
}

class SystemTrayDelegate : public ash::SystemTrayDelegate,
                           public SessionManagerClient::Observer,
                           public drive::JobListObserver,
                           public content::NotificationObserver,
                           public input_method::InputMethodManager::Observer,
                           public chromeos::LoginState::Observer,
                           public device::BluetoothAdapter::Observer,
                           public SystemKeyEventListener::CapsLockObserver,
                           public policy::CloudPolicyStore::Observer,
                           public ash::SessionStateObserver {
 public:
  SystemTrayDelegate()
      : ui_weak_ptr_factory_(
          new base::WeakPtrFactory<SystemTrayDelegate>(this)),
        user_profile_(NULL),
        clock_type_(base::GetHourClockType()),
        search_key_mapped_to_(input_method::kSearchKey),
        screen_locked_(false),
        have_session_start_time_(false),
        have_session_length_limit_(false),
        volume_control_delegate_(new VolumeController()) {
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
    registrar_->Add(
        this,
        chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER,
        content::NotificationService::AllSources());
    registrar_->Add(
        this,
        chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK,
        content::NotificationService::AllSources());
    registrar_->Add(
        this,
        chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE,
        content::NotificationService::AllSources());
  }

  virtual void Initialize() OVERRIDE {
    DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(this);

    input_method::InputMethodManager::Get()->AddObserver(this);
    UpdateClockType();

    if (SystemKeyEventListener::GetInstance())
      SystemKeyEventListener::GetInstance()->AddCapsLockObserver(this);

    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&SystemTrayDelegate::InitializeOnAdapterReady,
                   ui_weak_ptr_factory_->GetWeakPtr()));

    ash::Shell::GetInstance()->session_state_delegate()->
        AddSessionStateObserver(this);

    if (LoginState::IsInitialized())
      LoginState::Get()->AddObserver(this);
  }

  virtual void Shutdown() OVERRIDE {
  }

  void InitializeOnAdapterReady(
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
        base::Bind(&SystemTrayDelegate::UpdateSessionStartTime,
                   base::Unretained(this)));
    local_state_registrar_->Add(
        prefs::kSessionLengthLimit,
        base::Bind(&SystemTrayDelegate::UpdateSessionLengthLimit,
                   base::Unretained(this)));

    policy::BrowserPolicyConnector* policy_connector =
        g_browser_process->browser_policy_connector();
    policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
        policy_connector->GetDeviceCloudPolicyManager();
    if (policy_manager)
      policy_manager->core()->store()->AddObserver(this);
    UpdateEnterpriseDomain();
  }

  virtual ~SystemTrayDelegate() {
    // Unregister PrefChangeRegistrars.
    local_state_registrar_.reset();
    user_pref_registrar_.reset();

    // Unregister content notifications befure destroying any components.
    registrar_.reset();

    DBusThreadManager::Get()->GetSessionManagerClient()->RemoveObserver(this);
    input_method::InputMethodManager::Get()->RemoveObserver(this);
    if (SystemKeyEventListener::GetInstance())
      SystemKeyEventListener::GetInstance()->RemoveCapsLockObserver(this);
    bluetooth_adapter_->RemoveObserver(this);
    ash::Shell::GetInstance()->session_state_delegate()->
        RemoveSessionStateObserver(this);
    LoginState::Get()->RemoveObserver(this);

    // Stop observing Drive operations.
    UnobserveDriveUpdates();

    policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
        g_browser_process->browser_policy_connector()->
           GetDeviceCloudPolicyManager();
    if (policy_manager)
      policy_manager->core()->store()->RemoveObserver(this);
  }

  // Overridden from ash::SystemTrayDelegate:
  virtual bool GetTrayVisibilityOnStartup() OVERRIDE {
    // In case of OOBE / sign in screen tray will be shown later.
    return LoginState::Get()->IsUserLoggedIn();
  }

  virtual ash::user::LoginStatus GetUserLoginStatus() const OVERRIDE {
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
      case LoginState::LOGGED_IN_USER_LOCALLY_MANAGED:
        return ash::user::LOGGED_IN_LOCALLY_MANAGED;
      case LoginState::LOGGED_IN_USER_KIOSK_APP:
        return ash::user::LOGGED_IN_KIOSK_APP;
    }
    NOTREACHED();
    return ash::user::LOGGED_IN_NONE;
  }

  virtual bool IsOobeCompleted() const OVERRIDE {
    if (!base::SysInfo::IsRunningOnChromeOS() &&
        LoginState::Get()->IsUserLoggedIn())
      return true;
    return StartupUtils::IsOobeCompleted();
  }

  virtual void ChangeProfilePicture() OVERRIDE {
    content::RecordAction(
        content::UserMetricsAction("OpenChangeProfilePictureDialog"));
    ShowSettingsSubPageForAppropriateBrowser(
        chrome::kChangeProfilePictureSubPage);
  }

  virtual const std::string GetEnterpriseDomain() const OVERRIDE {
    return enterprise_domain_;
  }

  virtual const string16 GetEnterpriseMessage() const OVERRIDE {
    if (GetEnterpriseDomain().empty())
        return string16();
    return l10n_util::GetStringFUTF16(IDS_DEVICE_OWNED_BY_NOTICE,
                                      UTF8ToUTF16(GetEnterpriseDomain()));
  }

  virtual const std::string GetLocallyManagedUserManager() const OVERRIDE {
    if (GetUserLoginStatus() != ash::user::LOGGED_IN_LOCALLY_MANAGED)
      return std::string();
    return UserManager::Get()->GetSupervisedUserManager()->
        GetManagerDisplayEmail(
            chromeos::UserManager::Get()->GetActiveUser()->email());
  }

  virtual const string16 GetLocallyManagedUserManagerName() const OVERRIDE {
    if (GetUserLoginStatus() != ash::user::LOGGED_IN_LOCALLY_MANAGED)
      return string16();
    return UserManager::Get()->GetSupervisedUserManager()->
        GetManagerDisplayName(
            chromeos::UserManager::Get()->GetActiveUser()->email());
  }

  virtual const string16 GetLocallyManagedUserMessage() const OVERRIDE {
    if (GetUserLoginStatus() != ash::user::LOGGED_IN_LOCALLY_MANAGED)
        return string16();
    return l10n_util::GetStringFUTF16(
        IDS_USER_IS_LOCALLY_MANAGED_BY_NOTICE,
        UTF8ToUTF16(GetLocallyManagedUserManager()));
  }

  virtual bool SystemShouldUpgrade() const OVERRIDE {
    return UpgradeDetector::GetInstance()->notify_upgrade();
  }

  virtual base::HourClockType GetHourClockType() const OVERRIDE {
    return clock_type_;
  }

  virtual void ShowSettings() OVERRIDE {
    chrome::ScopedTabbedBrowserDisplayer displayer(
         ProfileManager::GetDefaultProfileOrOffTheRecord(),
         chrome::HOST_DESKTOP_TYPE_ASH);
    chrome::ShowSettings(displayer.browser());
  }

  virtual bool ShouldShowSettings() OVERRIDE {
    return UserManager::Get()->GetCurrentUserFlow()->ShouldShowSettings();
  }

  virtual void ShowDateSettings() OVERRIDE {
    content::RecordAction(content::UserMetricsAction("ShowDateOptions"));
    std::string sub_page = std::string(chrome::kSearchSubPage) + "#" +
        l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME);
    ShowSettingsSubPageForAppropriateBrowser(sub_page);
  }

  virtual void ShowNetworkSettings(const std::string& service_path) OVERRIDE {
    if (!LoginState::Get()->IsUserLoggedIn())
      return;
    ShowNetworkSettingsPage(service_path);
  }

  virtual void ShowBluetoothSettings() OVERRIDE {
    // TODO(sad): Make this work.
  }

  virtual void ShowDisplaySettings() OVERRIDE {
    content::RecordAction(content::UserMetricsAction("ShowDisplayOptions"));
    ShowSettingsSubPageForAppropriateBrowser(kDisplaySettingsSubPageName);
  }

  virtual void ShowChromeSlow() OVERRIDE {
    chrome::ScopedTabbedBrowserDisplayer displayer(
         ProfileManager::GetDefaultProfileOrOffTheRecord(),
         chrome::HOST_DESKTOP_TYPE_ASH);
    chrome::ShowSlow(displayer.browser());
  }

  virtual bool ShouldShowDisplayNotification() OVERRIDE {
    // Packaged app is not counted as 'last active', so if a browser opening the
    // display settings is in background of a packaged app, it will return true.
    // TODO(mukai): fix this.
    Browser* active_browser = chrome::FindLastActiveWithHostDesktopType(
        chrome::HOST_DESKTOP_TYPE_ASH);
    if (!active_browser)
      return true;

    content::WebContents* active_contents =
        active_browser->tab_strip_model()->GetActiveWebContents();
    if (!active_contents)
      return true;

    GURL visible_url = active_contents->GetLastCommittedURL();
    std::string display_settings_url =
        std::string(chrome::kChromeUISettingsURL) + kDisplaySettingsSubPageName;
    std::string display_overscan_url =
        std::string(chrome::kChromeUISettingsURL) +
        kDisplayOverscanSettingsSubPageName;
    return (visible_url.spec() != display_settings_url) &&
        (visible_url.spec() != display_overscan_url);
  }

  virtual void ShowDriveSettings() OVERRIDE {
    // TODO(tengs): Open the drive-specific settings page once we put it in.
    // For now just show Google Drive main page.
    chrome::ScopedTabbedBrowserDisplayer displayer(
         ProfileManager::GetDefaultProfileOrOffTheRecord(),
         chrome::HOST_DESKTOP_TYPE_ASH);
    chrome::ShowSingletonTabOverwritingNTP(
        displayer.browser(),
        chrome::GetSingletonTabNavigateParams(displayer.browser(),
                                              GURL(kDriveSettingsPageURL)));
  }

  virtual void ShowIMESettings() OVERRIDE {
    content::RecordAction(
        content::UserMetricsAction("OpenLanguageOptionsDialog"));
    ShowSettingsSubPageForAppropriateBrowser(chrome::kLanguageOptionsSubPage);
  }

  virtual void ShowHelp() OVERRIDE {
    chrome::ShowHelpForProfile(
        ProfileManager::GetDefaultProfileOrOffTheRecord(),
        chrome::HOST_DESKTOP_TYPE_ASH,
        chrome::HELP_SOURCE_MENU);
  }

  virtual void ShowAccessibilityHelp() OVERRIDE {
    chrome::ScopedTabbedBrowserDisplayer displayer(
         ProfileManager::GetDefaultProfileOrOffTheRecord(),
         chrome::HOST_DESKTOP_TYPE_ASH);
    accessibility::ShowAccessibilityHelp(displayer.browser());
  }

  virtual void ShowAccessibilitySettings() OVERRIDE {
    content::RecordAction(
        content::UserMetricsAction("ShowAccessibilitySettings"));
    std::string sub_page = std::string(chrome::kSearchSubPage) + "#" +
        l10n_util::GetStringUTF8(
            IDS_OPTIONS_SETTINGS_SECTION_TITLE_ACCESSIBILITY);
    ShowSettingsSubPageForAppropriateBrowser(sub_page);
  }

  virtual void ShowPublicAccountInfo() OVERRIDE {
    chrome::ScopedTabbedBrowserDisplayer displayer(
         ProfileManager::GetDefaultProfileOrOffTheRecord(),
         chrome::HOST_DESKTOP_TYPE_ASH);
    chrome::ShowPolicy(displayer.browser());
  }

  virtual void ShowLocallyManagedUserInfo() OVERRIDE {
    // TODO(antrim): find out what should we show in this case.
    // http://crbug.com/229762
  }

  virtual void ShowEnterpriseInfo() OVERRIDE {
    ash::user::LoginStatus status = GetUserLoginStatus();
    if (status == ash::user::LOGGED_IN_NONE ||
        status == ash::user::LOGGED_IN_LOCKED) {
      scoped_refptr<chromeos::HelpAppLauncher> help_app(
         new chromeos::HelpAppLauncher(GetNativeWindow()));
      help_app->ShowHelpTopic(chromeos::HelpAppLauncher::HELP_ENTERPRISE);
    } else {
      GURL url(google_util::StringAppendGoogleLocaleParam(
          chrome::kLearnMoreEnterpriseURL));
      chrome::ScopedTabbedBrowserDisplayer displayer(
          ProfileManager::GetDefaultProfileOrOffTheRecord(),
          chrome::HOST_DESKTOP_TYPE_ASH);
      chrome::ShowSingletonTab(displayer.browser(), url);
    }
  }

  virtual void ShowUserLogin() OVERRIDE {
    if (!ash::Shell::GetInstance()->delegate()->IsMultiProfilesEnabled())
      return;

    // Only regular users could add other users to current session.
    if (UserManager::Get()->GetActiveUser()->GetType() !=
            User::USER_TYPE_REGULAR) {
      return;
    }

    // TODO(nkostylev): Show some UI messages why no more users could be added
    // to this session. http://crbug.com/230863
    // We limit list of logged in users to 3 due to memory constraints.
    // TODO(nkostylev): Adjust this limitation based on device capabilites.
    // http://crbug.com/230865
    if (UserManager::Get()->GetLoggedInUsers().size() >= 3)
      return;

    // Launch sign in screen to add another user to current session.
    if (UserManager::Get()->GetUsersAdmittedForMultiProfile().size())
      UserAddingScreen::Get()->Start();
  }

  virtual void ShutDown() OVERRIDE {
    ash::Shell::GetInstance()->lock_state_controller()->RequestShutdown();
  }

  virtual void SignOut() OVERRIDE {
    chrome::AttemptUserExit();
  }

  virtual void RequestLockScreen() OVERRIDE {
    // TODO(antrim) : additional logging for crbug/173178
    LOG(WARNING) << "Requesting screen lock from AshSystemTrayDelegate";
    DBusThreadManager::Get()->GetSessionManagerClient()->RequestLockScreen();
  }

  virtual void RequestRestartForUpdate() OVERRIDE {
    // We expect that UpdateEngine is in "Reboot for update" state now.
    chrome::NotifyAndTerminate(true /* fast path */);
  }

  virtual void GetAvailableBluetoothDevices(
      ash::BluetoothDeviceList* list) OVERRIDE {
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

  virtual void BluetoothStartDiscovering() OVERRIDE {
    bluetooth_adapter_->StartDiscovering(
        base::Bind(&base::DoNothing),
        base::Bind(&BluetoothSetDiscoveringError));
  }

  virtual void BluetoothStopDiscovering() OVERRIDE {
    bluetooth_adapter_->StopDiscovering(
        base::Bind(&base::DoNothing),
        base::Bind(&BluetoothSetDiscoveringError));
  }

  virtual void ConnectToBluetoothDevice(const std::string& address) OVERRIDE {
    device::BluetoothDevice* device = bluetooth_adapter_->GetDevice(address);
    if (!device || device->IsConnecting() ||
        (device->IsConnected() && device->IsPaired())) {
      return;
    }
    if (device->IsPaired() && !device->IsConnectable())
      return;
    if (device->IsPaired() || !device->IsPairable()) {
      device->Connect(
          NULL,
          base::Bind(&base::DoNothing),
          base::Bind(&BluetoothDeviceConnectError));
    } else {  // Show paring dialog for the unpaired device.
      BluetoothPairingDialog* dialog =
          new BluetoothPairingDialog(GetNativeWindow(), device);
      // The dialog deletes itself on close.
      dialog->Show();
    }
  }

  virtual bool IsBluetoothDiscovering() OVERRIDE {
    return bluetooth_adapter_->IsDiscovering();
  }

  virtual void GetCurrentIME(ash::IMEInfo* info) OVERRIDE {
    input_method::InputMethodManager* manager =
        input_method::InputMethodManager::Get();
    input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
    input_method::InputMethodDescriptor ime = manager->GetCurrentInputMethod();
    ExtractIMEInfo(ime, *util, info);
    info->selected = true;
  }

  virtual void GetAvailableIMEList(ash::IMEInfoList* list) OVERRIDE {
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

  virtual void GetCurrentIMEProperties(
      ash::IMEPropertyInfoList* list) OVERRIDE {
    input_method::InputMethodManager* manager =
        input_method::InputMethodManager::Get();
    input_method::InputMethodPropertyList properties =
        manager->GetCurrentInputMethodProperties();
    for (size_t i = 0; i < properties.size(); ++i) {
      ash::IMEPropertyInfo property;
      property.key = properties[i].key;
      property.name = base::UTF8ToUTF16(properties[i].label);
      property.selected = properties[i].is_selection_item_checked;
      list->push_back(property);
    }
  }

  virtual void SwitchIME(const std::string& ime_id) OVERRIDE {
    input_method::InputMethodManager::Get()->ChangeInputMethod(ime_id);
  }

  virtual void ActivateIMEProperty(const std::string& key) OVERRIDE {
    input_method::InputMethodManager::Get()->
        ActivateInputMethodProperty(key);
  }

  virtual void CancelDriveOperation(int32 operation_id) OVERRIDE {
    DriveIntegrationService* integration_service =
        FindDriveIntegrationService();
    if (!integration_service)
      return;

    integration_service->job_list()->CancelJob(operation_id);
  }

  virtual void GetDriveOperationStatusList(
      ash::DriveOperationStatusList* list) OVERRIDE {
    DriveIntegrationService* integration_service =
        FindDriveIntegrationService();
    if (!integration_service)
      return;

    *list = ConvertToDriveStatusList(
        integration_service->job_list()->GetJobInfoList());
  }

  virtual void ShowNetworkConfigure(const std::string& network_id,
                                    gfx::NativeWindow parent_window) OVERRIDE {
    NetworkConfigView::Show(network_id, parent_window);
  }

  virtual bool EnrollNetwork(const std::string& network_id,
                             gfx::NativeWindow parent_window) OVERRIDE {
    return enrollment::CreateDialog(network_id, parent_window);
  }

  virtual void ManageBluetoothDevices() OVERRIDE {
    content::RecordAction(
        content::UserMetricsAction("ShowBluetoothSettingsPage"));
    std::string sub_page = std::string(chrome::kSearchSubPage) + "#" +
        l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_SECTION_TITLE_BLUETOOTH);
    ShowSettingsSubPageForAppropriateBrowser(sub_page);
  }

  virtual void ToggleBluetooth() OVERRIDE {
    bluetooth_adapter_->SetPowered(!bluetooth_adapter_->IsPowered(),
                                   base::Bind(&base::DoNothing),
                                   base::Bind(&BluetoothPowerFailure));
  }

  virtual void ShowMobileSimDialog() OVERRIDE {
    SimDialogDelegate::ShowDialog(GetNativeWindow(),
                                  SimDialogDelegate::SIM_DIALOG_UNLOCK);
  }

  virtual void ShowMobileSetupDialog(const std::string& service_path) OVERRIDE {
    MobileSetupDialog::Show(service_path);
  }

  virtual void ShowOtherNetworkDialog(const std::string& type) OVERRIDE {
    if (type == shill::kTypeCellular) {
      ChooseMobileNetworkDialog::ShowDialog(GetNativeWindow());
      return;
    }
    NetworkConfigView::ShowForType(type, GetNativeWindow());
  }

  virtual bool GetBluetoothAvailable() OVERRIDE {
    return bluetooth_adapter_->IsPresent();
  }

  virtual bool GetBluetoothEnabled() OVERRIDE {
    return bluetooth_adapter_->IsPowered();
  }

  virtual void ChangeProxySettings() OVERRIDE {
    CHECK(GetUserLoginStatus() == ash::user::LOGGED_IN_NONE);
    LoginDisplayHostImpl::default_host()->OpenProxySettings();
  }

  virtual ash::VolumeControlDelegate*
  GetVolumeControlDelegate() const OVERRIDE {
    return volume_control_delegate_.get();
  }

  virtual void SetVolumeControlDelegate(
      scoped_ptr<ash::VolumeControlDelegate> delegate) OVERRIDE {
    volume_control_delegate_.swap(delegate);
  }

  virtual bool GetSessionStartTime(
      base::TimeTicks* session_start_time) OVERRIDE {
    *session_start_time = session_start_time_;
    return have_session_start_time_;
  }

  virtual bool GetSessionLengthLimit(
      base::TimeDelta* session_length_limit) OVERRIDE {
    *session_length_limit = session_length_limit_;
    return have_session_length_limit_;
  }

  virtual int GetSystemTrayMenuWidth() OVERRIDE {
    return l10n_util::GetLocalizedContentsWidthInPixels(
        IDS_SYSTEM_TRAY_MENU_BUBBLE_WIDTH_PIXELS);
  }

  virtual void MaybeSpeak(const std::string& utterance) const OVERRIDE {
    AccessibilityManager::Get()->MaybeSpeak(utterance);
  }

 private:
  ash::SystemTray* GetPrimarySystemTray() {
    return ash::Shell::GetInstance()->GetPrimarySystemTray();
  }

  ash::SystemTrayNotifier* GetSystemTrayNotifier() {
    return ash::Shell::GetInstance()->system_tray_notifier();
  }

  void SetProfile(Profile* profile) {
    // Stop observing the current |user_profile_| on Drive integration status.
    UnobserveDriveUpdates();

    user_profile_ = profile;

    // Restart observation, now for the newly set |profile|.
    ObserveDriveUpdates();

    PrefService* prefs = profile->GetPrefs();
    user_pref_registrar_.reset(new PrefChangeRegistrar);
    user_pref_registrar_->Init(prefs);
    user_pref_registrar_->Add(
        prefs::kUse24HourClock,
        base::Bind(&SystemTrayDelegate::UpdateClockType,
                   base::Unretained(this)));
    user_pref_registrar_->Add(
        prefs::kLanguageRemapSearchKeyTo,
        base::Bind(&SystemTrayDelegate::OnLanguageRemapSearchKeyToChanged,
                   base::Unretained(this)));
    user_pref_registrar_->Add(
        prefs::kShowLogoutButtonInTray,
        base::Bind(&SystemTrayDelegate::UpdateShowLogoutButtonInTray,
                   base::Unretained(this)));
    user_pref_registrar_->Add(
        prefs::kLargeCursorEnabled,
        base::Bind(&SystemTrayDelegate::OnAccessibilityModeChanged,
                   base::Unretained(this),
                   ash::A11Y_NOTIFICATION_NONE));
    user_pref_registrar_->Add(
        prefs::kAutoclickEnabled,
        base::Bind(&SystemTrayDelegate::OnAccessibilityModeChanged,
                   base::Unretained(this),
                   ash::A11Y_NOTIFICATION_NONE));
    user_pref_registrar_->Add(
        prefs::kShouldAlwaysShowAccessibilityMenu,
        base::Bind(&SystemTrayDelegate::OnAccessibilityModeChanged,
                   base::Unretained(this),
                   ash::A11Y_NOTIFICATION_NONE));
    user_pref_registrar_->Add(
        prefs::kPerformanceTracingEnabled,
        base::Bind(&SystemTrayDelegate::UpdatePerformanceTracing,
                   base::Unretained(this)));

    UpdateClockType();
    UpdateShowLogoutButtonInTray();
    UpdatePerformanceTracing();
    search_key_mapped_to_ =
        profile->GetPrefs()->GetInteger(prefs::kLanguageRemapSearchKeyTo);
  }

  bool UnsetProfile(Profile* profile) {
    if (profile != user_profile_)
      return false;
    user_pref_registrar_.reset();
    return true;
  }

  void ObserveDriveUpdates() {
    DriveIntegrationService* integration_service =
        FindDriveIntegrationService();
    if (integration_service)
      integration_service->job_list()->AddObserver(this);
  }

  void UnobserveDriveUpdates() {
    DriveIntegrationService* integration_service =
        FindDriveIntegrationService();
    if (integration_service)
      integration_service->job_list()->RemoveObserver(this);
  }

  bool ShouldUse24HourClock() const {
    // On login screen and in guest mode owner default is used for
    // kUse24HourClock preference.
    const ash::user::LoginStatus status = GetUserLoginStatus();
    const CrosSettings* const cros_settings = CrosSettings::Get();
    bool system_use_24_hour_clock = true;
    const bool system_value_found = cros_settings->GetBoolean(
        kSystemUse24HourClock, &system_use_24_hour_clock);

    if (status == ash::user::LOGGED_IN_NONE)
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

  void UpdateClockType() {
    if (!user_pref_registrar_)
      return;

    const bool use_24_hour_clock = ShouldUse24HourClock();
    clock_type_ = use_24_hour_clock ? base::k24HourClock : base::k12HourClock;
    GetSystemTrayNotifier()->NotifyDateFormatChanged();
    // This also works for enterprise-managed devices because they never have
    // local owner.
    if (chromeos::UserManager::Get()->IsCurrentUserOwner())
      CrosSettings::Get()->SetBoolean(kSystemUse24HourClock, use_24_hour_clock);
  }

  void UpdateShowLogoutButtonInTray() {
    GetSystemTrayNotifier()->NotifyShowLoginButtonChanged(
        user_pref_registrar_->prefs()->GetBoolean(
            prefs::kShowLogoutButtonInTray));
  }

  void UpdateSessionStartTime() {
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

  void UpdateSessionLengthLimit() {
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

  // LoginState::Observer overrides.
  virtual void LoggedInStateChanged() OVERRIDE {
    UpdateClockType();
  }

  // Overridden from SessionManagerClient::Observer.
  virtual void LockScreen() OVERRIDE {
    screen_locked_ = true;
    ash::Shell::GetInstance()->UpdateAfterLoginStatusChange(
        GetUserLoginStatus());
  }

  virtual void ScreenIsUnlocked() OVERRIDE {
    screen_locked_ = false;
    ash::Shell::GetInstance()->UpdateAfterLoginStatusChange(
        GetUserLoginStatus());
  }

  gfx::NativeWindow GetNativeWindow() const {
    bool session_started = ash::Shell::GetInstance()->session_state_delegate()
        ->IsActiveUserSessionStarted();
    return GetNativeWindowByStatus(GetUserLoginStatus(), session_started);
  }

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
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
        SetProfile(ProfileManager::GetDefaultProfile());
        break;
      }
      case chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK:
      case chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE:
      case chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER: {
        AccessibilityStatusEventDetails* accessibility_status =
            content::Details<AccessibilityStatusEventDetails>(details).ptr();
        OnAccessibilityModeChanged(accessibility_status->notify);
        break;
      }
      default:
        NOTREACHED();
    }
  }

  void OnLanguageRemapSearchKeyToChanged() {
    search_key_mapped_to_ = user_pref_registrar_->prefs()->GetInteger(
        prefs::kLanguageRemapSearchKeyTo);
  }

  void OnAccessibilityModeChanged(
      ash::AccessibilityNotificationVisibility notify) {
    GetSystemTrayNotifier()->NotifyAccessibilityModeChanged(notify);
  }

  void UpdatePerformanceTracing() {
    if (!user_pref_registrar_)
      return;
    bool value =
        user_pref_registrar_->prefs()->GetBoolean(
            prefs::kPerformanceTracingEnabled);
    GetSystemTrayNotifier()->NotifyTracingModeChanged(value);
  }

  // Overridden from InputMethodManager::Observer.
  virtual void InputMethodChanged(
      input_method::InputMethodManager* manager, bool show_message) OVERRIDE {
    // |show_message| in ash means the message_center notifications which should
    // not be shown when kEnableIMEModeIndicator is on, since the mode indicator
    // already notifies the user.
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableIMEModeIndicator)) {
      show_message = false;
    }
    GetSystemTrayNotifier()->NotifyRefreshIME(show_message);
  }

  virtual void InputMethodPropertyChanged(
      input_method::InputMethodManager* manager) OVERRIDE {
    GetSystemTrayNotifier()->NotifyRefreshIME(false);
  }

  // drive::JobListObserver overrides.
  virtual void OnJobAdded(const drive::JobInfo& job_info) OVERRIDE {
    OnJobUpdated(job_info);
  }

  virtual void OnJobDone(const drive::JobInfo& job_info,
                         drive::FileError error) OVERRIDE {
    ash::DriveOperationStatus status;
    if (ConvertToFinishedDriveOperationStatus(job_info, error, &status))
      GetSystemTrayNotifier()->NotifyDriveJobUpdated(status);
  }

  virtual void OnJobUpdated(const drive::JobInfo& job_info) OVERRIDE {
    ash::DriveOperationStatus status;
    if (ConvertToDriveOperationStatus(job_info, &status))
      GetSystemTrayNotifier()->NotifyDriveJobUpdated(status);
  }

  DriveIntegrationService* FindDriveIntegrationService() {
    return user_profile_ ?
        DriveIntegrationServiceFactory::FindForProfile(user_profile_) : NULL;
  }

  // Overridden from BluetoothAdapter::Observer.
  virtual void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                     bool present) OVERRIDE {
    GetSystemTrayNotifier()->NotifyRefreshBluetooth();
  }

  virtual void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                     bool powered) OVERRIDE {
    GetSystemTrayNotifier()->NotifyRefreshBluetooth();
  }

  virtual void AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                         bool discovering) OVERRIDE {
    GetSystemTrayNotifier()->NotifyBluetoothDiscoveringChanged();
  }

  virtual void DeviceAdded(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device) OVERRIDE {
    GetSystemTrayNotifier()->NotifyRefreshBluetooth();
  }

  virtual void DeviceChanged(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* device) OVERRIDE {
    GetSystemTrayNotifier()->NotifyRefreshBluetooth();
  }

  virtual void DeviceRemoved(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* device) OVERRIDE {
    GetSystemTrayNotifier()->NotifyRefreshBluetooth();
  }

  // Overridden from SystemKeyEventListener::CapsLockObserver.
  virtual void OnCapsLockChange(bool enabled) OVERRIDE {
    bool search_mapped_to_caps_lock = false;
    if (!base::SysInfo::IsRunningOnChromeOS() ||
        search_key_mapped_to_ == input_method::kCapsLockKey)
      search_mapped_to_caps_lock = true;
    GetSystemTrayNotifier()->NotifyCapsLockChanged(
        enabled, search_mapped_to_caps_lock);
  }

  void UpdateEnterpriseDomain() {
    std::string enterprise_domain =
        g_browser_process->browser_policy_connector()->GetEnterpriseDomain();
    if (enterprise_domain_ != enterprise_domain) {
       enterprise_domain_ = enterprise_domain;
       GetSystemTrayNotifier()->NotifyEnterpriseDomainChanged();
    }
  }

  // Overridden from CloudPolicyStore::Observer
  virtual void OnStoreLoaded(policy::CloudPolicyStore* store) OVERRIDE {
    UpdateEnterpriseDomain();
  }

  virtual void OnStoreError(policy::CloudPolicyStore* store) OVERRIDE {
    UpdateEnterpriseDomain();
  }

  // Overridden from ash::SessionStateObserver
  virtual void ActiveUserChanged(const std::string& user_id) OVERRIDE {
    GetSystemTrayNotifier()->NotifyUserUpdate();
  }

  virtual void UserAddedToSession(const std::string& user_id) OVERRIDE {
    GetSystemTrayNotifier()->NotifyUserAddedToSession();
  }

  scoped_ptr<base::WeakPtrFactory<SystemTrayDelegate> > ui_weak_ptr_factory_;
  scoped_ptr<content::NotificationRegistrar> registrar_;
  scoped_ptr<PrefChangeRegistrar> local_state_registrar_;
  scoped_ptr<PrefChangeRegistrar> user_pref_registrar_;
  Profile* user_profile_;
  base::HourClockType clock_type_;
  int search_key_mapped_to_;
  bool screen_locked_;
  bool have_session_start_time_;
  base::TimeTicks session_start_time_;
  bool have_session_length_limit_;
  base::TimeDelta session_length_limit_;
  std::string enterprise_domain_;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  scoped_ptr<ash::VolumeControlDelegate> volume_control_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegate);
};

}  // namespace

ash::SystemTrayDelegate* CreateSystemTrayDelegate() {
  return new chromeos::SystemTrayDelegate();
}

}  // namespace chromeos
