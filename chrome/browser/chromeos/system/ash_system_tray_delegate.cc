// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/ash_system_tray_delegate.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/audio/audio_observer.h"
#include "ash/system/bluetooth/bluetooth_observer.h"
#include "ash/system/brightness/brightness_observer.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/chromeos/network/network_tray_delegate.h"
#include "ash/system/date/clock_observer.h"
#include "ash/system/drive/drive_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/logout_button/logout_button_observer.h"
#include "ash/system/power/power_status_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/tray_caps_lock.h"
#include "ash/system/user/update_observer.h"
#include "ash/system/user/user_observer.h"
#include "ash/volume_control_delegate.h"
#include "ash/wm/session_state_controller.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/audio/audio_handler.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_pairing_dialog.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/status/data_promo_notification.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/chromeos/status/network_menu_icon.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chrome/browser/chromeos/system_key_event_listener.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/volume_controller_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/system_clock_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "grit/ash_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

using drive::DriveSystemService;
using drive::DriveSystemServiceFactory;

namespace chromeos {

namespace {

// The minimum session length limit that can be set.
const int kSessionLengthLimitMinMs = 30 * 1000;  // 30 seconds.

// The maximum session length limit that can be set.
const int kSessionLengthLimitMaxMs = 24 * 60 * 60 * 1000;  // 24 hours.

// Time delay for rechecking gdata operation when we suspect that there will
// be no upcoming activity notifications that need to be pushed to UI.
const int kGDataOperationRecheckDelayMs = 5000;

ash::NetworkIconInfo CreateNetworkIconInfo(const Network* network) {
  ash::NetworkIconInfo info;
  info.name = network->type() == TYPE_ETHERNET ?
      l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET) :
      UTF8ToUTF16(network->name());
  info.image = NetworkMenuIcon::GetImage(network, NetworkMenuIcon::COLOR_DARK);
  info.service_path = network->service_path();
  info.connecting = network->connecting();
  info.connected = network->connected();
  info.is_cellular = network->type() == TYPE_CELLULAR;
  return info;
}

void ExtractIMEInfo(const input_method::InputMethodDescriptor& ime,
                    const input_method::InputMethodUtil& util,
                    ash::IMEInfo* info) {
  info->id = ime.id();
  info->name = util.GetInputMethodLongName(ime);
  info->medium_name = util.GetInputMethodMediumName(ime);
  info->short_name = util.GetInputMethodShortName(ime);
  info->third_party = ime.third_party();
}

gfx::NativeWindow GetNativeWindowByStatus(
    ash::user::LoginStatus login_status) {
  int container_id =
      (login_status == ash::user::LOGGED_IN_NONE ||
       login_status == ash::user::LOGGED_IN_LOCKED) ?
           ash::internal::kShellWindowId_LockSystemModalContainer :
           ash::internal::kShellWindowId_SystemModalContainer;
  return ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                  container_id);
}

ash::DriveOperationStatusList GetDriveStatusList(
    const google_apis::OperationProgressStatusList& list) {
  ash::DriveOperationStatusList results;
  for (google_apis::OperationProgressStatusList::const_iterator it =
           list.begin();
       it != list.end(); ++it) {
    ash::DriveOperationStatus status;
    status.file_path = it->file_path;
    status.progress = it->progress_total == 0 ? 0.0 :
        static_cast<double>(it->progress_current) /
            static_cast<double>(it->progress_total);
    status.type = static_cast<ash::DriveOperationStatus::OperationType>(
        it->operation_type);
    status.state = static_cast<ash::DriveOperationStatus::OperationState>(
        it->transfer_state);
    results.push_back(status);
  }
  return results;
}

void BluetoothPowerFailure() {
  // TODO(sad): Show an error bubble?
}

void BluetoothDiscoveryFailure() {
  // TODO(sad): Show an error bubble?
}

void BluetoothDeviceDisconnectError() {
  // TODO(sad): Do something?
}

void BluetoothSetDiscoveringError() {
  LOG(ERROR) << "BluetoothSetDiscovering failed.";
}

void BluetoothDeviceConnectError(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  // TODO(sad): Do something?
}

ash::NetworkObserver::NetworkType NetworkTypeForCellular(
    const CellularNetwork* cellular) {
  if (cellular->network_technology() == NETWORK_TECHNOLOGY_LTE ||
      cellular->network_technology() == NETWORK_TECHNOLOGY_LTE_ADVANCED)
    return ash::NetworkObserver::NETWORK_CELLULAR_LTE;
  return ash::NetworkObserver::NETWORK_CELLULAR;
}

class SystemTrayDelegate : public ash::SystemTrayDelegate,
                           public AudioHandler::VolumeObserver,
                           public PowerManagerClient::Observer,
                           public SessionManagerClient::Observer,
                           public NetworkMenuIcon::Delegate,
                           public NetworkMenu::Delegate,
                           public NetworkLibrary::NetworkManagerObserver,
                           public NetworkLibrary::NetworkObserver,
                           public google_apis::DriveServiceObserver,
                           public content::NotificationObserver,
                           public input_method::InputMethodManager::Observer,
                           public system::TimezoneSettings::Observer,
                           public chromeos::SystemClockClient::Observer,
                           public device::BluetoothAdapter::Observer,
                           public SystemKeyEventListener::CapsLockObserver,
                           public ash::NetworkTrayDelegate,
                           public policy::CloudPolicyStore::Observer {
 public:
  SystemTrayDelegate()
      : ui_weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(
            new base::WeakPtrFactory<SystemTrayDelegate>(this))),
        network_icon_(ALLOW_THIS_IN_INITIALIZER_LIST(
                      new NetworkMenuIcon(this, NetworkMenuIcon::MENU_MODE))),
        network_icon_dark_(ALLOW_THIS_IN_INITIALIZER_LIST(
                      new NetworkMenuIcon(this, NetworkMenuIcon::MENU_MODE))),
        network_icon_vpn_(ALLOW_THIS_IN_INITIALIZER_LIST(
                      new NetworkMenuIcon(this, NetworkMenuIcon::MENU_MODE))),
        network_menu_(ALLOW_THIS_IN_INITIALIZER_LIST(new NetworkMenu(this))),
        clock_type_(base::k24HourClock),
        search_key_mapped_to_(input_method::kSearchKey),
        screen_locked_(false),
        data_promo_notification_(new DataPromoNotification()),
        cellular_activating_(false),
        cellular_out_of_credits_(false),
        volume_control_delegate_(new VolumeController()) {
    // Register notifications on construction so that events such as
    // PROFILE_CREATED do not get missed if they happen before Initialize().
    registrar_.Add(this,
                   chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                   content::NotificationService::AllSources());
    if (GetUserLoginStatus() == ash::user::LOGGED_IN_NONE) {
      registrar_.Add(this,
                     chrome::NOTIFICATION_SESSION_STARTED,
                     content::NotificationService::AllSources());
    }
    registrar_.Add(this,
                   chrome::NOTIFICATION_PROFILE_CREATED,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                   content::NotificationService::AllSources());
    registrar_.Add(
        this,
        chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER,
        content::NotificationService::AllSources());
    registrar_.Add(
        this,
        chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK,
        content::NotificationService::AllSources());
    registrar_.Add(
        this,
        chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE,
        content::NotificationService::AllSources());
  }

  virtual void Initialize() OVERRIDE {
    AudioHandler::GetInstance()->AddVolumeObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestStatusUpdate(
        PowerManagerClient::UPDATE_INITIAL);
    DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(this);

    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    crosnet->AddNetworkManagerObserver(this);
    OnNetworkManagerChanged(crosnet);

    input_method::GetInputMethodManager()->AddObserver(this);

    system::TimezoneSettings::GetInstance()->AddObserver(this);
    DBusThreadManager::Get()->GetSystemClockClient()->AddObserver(this);

    if (SystemKeyEventListener::GetInstance())
      SystemKeyEventListener::GetInstance()->AddCapsLockObserver(this);

    network_icon_->SetResourceColorTheme(NetworkMenuIcon::COLOR_LIGHT);
    network_icon_dark_->SetResourceColorTheme(NetworkMenuIcon::COLOR_DARK);
    network_icon_vpn_->SetResourceColorTheme(NetworkMenuIcon::COLOR_DARK);

    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&SystemTrayDelegate::InitializeOnAdapterReady,
                   ui_weak_ptr_factory_->GetWeakPtr()));
  }

  void InitializeOnAdapterReady(
      scoped_refptr<device::BluetoothAdapter> adapter) {
    bluetooth_adapter_ = adapter;
    CHECK(bluetooth_adapter_);
    bluetooth_adapter_->AddObserver(this);

    local_state_registrar_.Init(g_browser_process->local_state());

    UpdateSessionStartTime();
    UpdateSessionLengthLimit();

    local_state_registrar_.Add(
        prefs::kSessionStartTime,
        base::Bind(&SystemTrayDelegate::UpdateSessionStartTime,
                   base::Unretained(this)));
    local_state_registrar_.Add(
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
    AudioHandler* audiohandler = AudioHandler::GetInstance();
    if (audiohandler)
      audiohandler->RemoveVolumeObserver(this);
    DBusThreadManager::Get()->GetSessionManagerClient()->RemoveObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
    DBusThreadManager::Get()->GetSystemClockClient()->RemoveObserver(this);
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    if (crosnet)
      crosnet->RemoveNetworkManagerObserver(this);
    input_method::GetInputMethodManager()->RemoveObserver(this);
    system::TimezoneSettings::GetInstance()->RemoveObserver(this);
    if (SystemKeyEventListener::GetInstance())
      SystemKeyEventListener::GetInstance()->RemoveCapsLockObserver(this);
    bluetooth_adapter_->RemoveObserver(this);

    // Stop observing gdata operations.
    DriveSystemService* system_service = FindDriveSystemService();
    if (system_service) {
      system_service->drive_service()->RemoveObserver(this);
    }

    policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
        g_browser_process->browser_policy_connector()->
           GetDeviceCloudPolicyManager();
    if (policy_manager)
      policy_manager->core()->store()->RemoveObserver(this);
  }

  // Overridden from ash::SystemTrayDelegate:
  virtual bool GetTrayVisibilityOnStartup() OVERRIDE {
    // In case of OOBE / sign in screen tray will be shown later.
    return UserManager::Get()->IsUserLoggedIn();
  }

  virtual const string16 GetUserDisplayName() const OVERRIDE {
    return UserManager::Get()->GetLoggedInUser()->GetDisplayName();
  }

  virtual const std::string GetUserEmail() const OVERRIDE {
    return UserManager::Get()->GetLoggedInUser()->display_email();
  }

  virtual const gfx::ImageSkia& GetUserImage() const OVERRIDE {
    return UserManager::Get()->GetLoggedInUser()->image();
  }

  virtual ash::user::LoginStatus GetUserLoginStatus() const OVERRIDE {
    UserManager* manager = UserManager::Get();
    // At new user image screen manager->IsUserLoggedIn() would return true
    // but there's no browser session available yet so use SessionStarted().
    if (!manager->IsSessionStarted())
      return ash::user::LOGGED_IN_NONE;
    if (screen_locked_)
      return ash::user::LOGGED_IN_LOCKED;
    if (manager->IsCurrentUserOwner())
      return ash::user::LOGGED_IN_OWNER;
    if (manager->IsLoggedInAsGuest())
      return ash::user::LOGGED_IN_GUEST;
    if (manager->IsLoggedInAsDemoUser())
      return ash::user::LOGGED_IN_RETAIL_MODE;
    if (manager->IsLoggedInAsPublicAccount())
      return ash::user::LOGGED_IN_PUBLIC;
    return ash::user::LOGGED_IN_USER;
  }

  virtual void ChangeProfilePicture() OVERRIDE {
    content::RecordAction(
        content::UserMetricsAction("OpenChangeProfilePictureDialog"));
    chrome::ShowSettingsSubPage(GetAppropriateBrowser(),
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

  virtual bool SystemShouldUpgrade() const OVERRIDE {
    return UpgradeDetector::GetInstance()->notify_upgrade();
  }

  virtual base::HourClockType GetHourClockType() const OVERRIDE {
    return clock_type_;
  }

  virtual PowerSupplyStatus GetPowerSupplyStatus() const OVERRIDE {
    return power_supply_status_;
  }

  virtual void RequestStatusUpdate() const OVERRIDE {
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestStatusUpdate(
        PowerManagerClient::UPDATE_USER);
  }

  virtual void ShowSettings() OVERRIDE {
    chrome::ShowSettings(GetAppropriateBrowser());
  }

  virtual void ShowDateSettings() OVERRIDE {
    content::RecordAction(content::UserMetricsAction("ShowDateOptions"));
    std::string sub_page = std::string(chrome::kSearchSubPage) + "#" +
        l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME);
    chrome::ShowSettingsSubPage(GetAppropriateBrowser(), sub_page);
  }

  virtual void ShowNetworkSettings() OVERRIDE {
    content::RecordAction(
        content::UserMetricsAction("OpenInternetOptionsDialog"));
    chrome::ShowSettingsSubPage(GetAppropriateBrowser(),
                                chrome::kInternetOptionsSubPage);
  }

  virtual void ShowBluetoothSettings() OVERRIDE {
    // TODO(sad): Make this work.
  }

  virtual void ShowDisplaySettings() OVERRIDE {
    content::RecordAction(content::UserMetricsAction("ShowDisplayOptions"));
    chrome::ShowSettingsSubPage(GetAppropriateBrowser(), "display");
  }

  virtual void ShowDriveSettings() OVERRIDE {
    // TODO(hshi): Open the drive-specific settings page once we put it in.
    // For now just show search result for downoads settings.
    std::string sub_page = std::string(chrome::kSearchSubPage) + "#" +
        l10n_util::GetStringUTF8(IDS_OPTIONS_DOWNLOADLOCATION_GROUP_NAME);
    chrome::ShowSettingsSubPage(GetAppropriateBrowser(), sub_page);
  }

  virtual void ShowIMESettings() OVERRIDE {
    content::RecordAction(
        content::UserMetricsAction("OpenLanguageOptionsDialog"));
    chrome::ShowSettingsSubPage(GetAppropriateBrowser(),
                                chrome::kLanguageOptionsSubPage);
  }

  virtual void ShowHelp() OVERRIDE {
    chrome::ShowHelp(GetAppropriateBrowser(), chrome::HELP_SOURCE_MENU);
  }

  virtual void ShowAccessibilityHelp() OVERRIDE {
    accessibility::ShowAccessibilityHelp(GetAppropriateBrowser());
  }

  virtual void ShowPublicAccountInfo() OVERRIDE {
    chrome::ShowPolicy(GetAppropriateBrowser());
  }

  virtual void ShowEnterpriseInfo() OVERRIDE {
    ash::user::LoginStatus status = GetUserLoginStatus();
    if (status == ash::user::LOGGED_IN_NONE ||
        status == ash::user::LOGGED_IN_LOCKED) {
      scoped_refptr<chromeos::HelpAppLauncher> help_app(
         new chromeos::HelpAppLauncher(
            GetNativeWindowByStatus(GetUserLoginStatus())));
      help_app->ShowHelpTopic(chromeos::HelpAppLauncher::HELP_ENTERPRISE);
    } else {
      GURL url(google_util::StringAppendGoogleLocaleParam(
          chrome::kLearnMoreEnterpriseURL));
      chrome::ShowSingletonTab(GetAppropriateBrowser(), url);
    }
  }

  virtual void ShutDown() OVERRIDE {
    ash::Shell::GetInstance()->session_state_controller()->RequestShutdown();
  }

  virtual void SignOut() OVERRIDE {
    chrome::AttemptUserExit();
  }

  virtual void RequestLockScreen() OVERRIDE {
    // TODO(antrim) : additional logging for crbug/173178
    LOG(WARNING) << "Requesting screen lock from AshSystemTrayDelegate";
    DBusThreadManager::Get()->GetSessionManagerClient()->RequestLockScreen();
  }

  virtual void RequestRestart() OVERRIDE {
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
  }

  virtual void GetAvailableBluetoothDevices(
      ash::BluetoothDeviceList* list) OVERRIDE {
    device::BluetoothAdapter::DeviceList devices =
        bluetooth_adapter_->GetDevices();
    for (size_t i = 0; i < devices.size(); ++i) {
      device::BluetoothDevice* device = devices[i];
      ash::BluetoothDeviceInfo info;
      info.address = device->address();
      info.display_name = device->GetName();
      info.connected = device->IsConnected();
      info.paired = device->IsPaired();
      info.visible = device->IsVisible();
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

  virtual void ToggleBluetoothConnection(const std::string& address) OVERRIDE {
    device::BluetoothDevice* device = bluetooth_adapter_->GetDevice(address);
    if (!device)
      return;
    if (device->IsConnected()) {
      device->Disconnect(
          base::Bind(&base::DoNothing),
          base::Bind(&BluetoothDeviceDisconnectError));
    } else if (device->IsPaired()) {
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
        input_method::GetInputMethodManager();
    input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
    input_method::InputMethodDescriptor ime = manager->GetCurrentInputMethod();
    ExtractIMEInfo(ime, *util, info);
    info->selected = true;
  }

  virtual void GetAvailableIMEList(ash::IMEInfoList* list) OVERRIDE {
    input_method::InputMethodManager* manager =
        input_method::GetInputMethodManager();
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
        input_method::GetInputMethodManager();
    input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
    input_method::InputMethodPropertyList properties =
        manager->GetCurrentInputMethodProperties();
    for (size_t i = 0; i < properties.size(); ++i) {
      ash::IMEPropertyInfo property;
      property.key = properties[i].key;
      property.name = util->TranslateString(properties[i].label);
      property.selected = properties[i].is_selection_item_checked;
      list->push_back(property);
    }
  }

  virtual void SwitchIME(const std::string& ime_id) OVERRIDE {
    input_method::GetInputMethodManager()->ChangeInputMethod(ime_id);
  }

  virtual void ActivateIMEProperty(const std::string& key) OVERRIDE {
    input_method::GetInputMethodManager()->
        ActivateInputMethodProperty(key);
  }

  virtual void CancelDriveOperation(const base::FilePath& file_path) OVERRIDE {
    DriveSystemService* system_service = FindDriveSystemService();
    if (!system_service)
      return;

    system_service->drive_service()->CancelForFilePath(file_path);
  }

  virtual void GetDriveOperationStatusList(
      ash::DriveOperationStatusList* list) OVERRIDE {
    DriveSystemService* system_service = FindDriveSystemService();
    if (!system_service)
      return;

    *list = GetDriveStatusList(
        system_service->drive_service()->GetProgressStatusList());
  }


  virtual void GetMostRelevantNetworkIcon(ash::NetworkIconInfo* info,
                                          bool dark) OVERRIDE {
    NetworkMenuIcon* icon =
        dark ? network_icon_dark_.get() : network_icon_.get();
    info->image = icon->GetIconAndText(&info->description);
    info->tray_icon_visible = icon->ShouldShowIconInTray();
  }

  virtual void GetVirtualNetworkIcon(ash::NetworkIconInfo* info) OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    if (crosnet->virtual_network_connected()) {
      NetworkMenuIcon* icon = network_icon_vpn_.get();
      info->image = icon->GetVpnIconAndText(&info->description);
      info->tray_icon_visible = false;
    } else {
      gfx::ImageSkia* image = NetworkMenuIcon::GetVirtualNetworkImage();
      info->image = *image;
      info->description = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_VPN_DISCONNECTED);
    }
  }

  virtual void GetAvailableNetworks(
      std::vector<ash::NetworkIconInfo>* list) OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();

    std::set<const Network*> added;

    // Add the active network first.
    if (crosnet->active_nonvirtual_network())
      AddNetworkToList(list, &added, crosnet->active_nonvirtual_network());

    if (crosnet->ethernet_network() &&
        crosnet->ethernet_network()->connecting_or_connected()) {
      AddNetworkToList(list, &added, crosnet->ethernet_network());
    }
    if (crosnet->cellular_network()
        && crosnet->cellular_network()->connecting_or_connected()) {
      AddNetworkToList(list, &added, crosnet->cellular_network());
    }
    if (crosnet->wimax_network()
        && crosnet->wimax_network()->connecting_or_connected()) {
      AddNetworkToList(list, &added, crosnet->wimax_network());
    }
    if (crosnet->wifi_network()
        && crosnet->wifi_network()->connecting_or_connected()) {
      AddNetworkToList(list, &added, crosnet->wifi_network());
    }

    // Add remaining networks by type.

    // Ethernet.
    if (crosnet->ethernet_available() && crosnet->ethernet_enabled()) {
      const EthernetNetwork* ethernet_network = crosnet->ethernet_network();
      if (ethernet_network)
        AddNetworkToList(list, &added, ethernet_network);
    }

    // Cellular.
    if (crosnet->cellular_available() && crosnet->cellular_enabled()) {
      const CellularNetworkVector& cell = crosnet->cellular_networks();
      for (size_t i = 0; i < cell.size(); ++i)
        AddNetworkToList(list, &added, cell[i]);
    }

    // Wimax.
    if (crosnet->wimax_available() && crosnet->wimax_enabled()) {
      const WimaxNetworkVector& wimax = crosnet->wimax_networks();
      for (size_t i = 0; i < wimax.size(); ++i)
        AddNetworkToList(list, &added, wimax[i]);
    }

    // Wifi.
    if (crosnet->wifi_available() && crosnet->wifi_enabled()) {
      const WifiNetworkVector& wifi = crosnet->wifi_networks();
      for (size_t i = 0; i < wifi.size(); ++i)
        AddNetworkToList(list, &added, wifi[i]);
    }
  }

  virtual void GetVirtualNetworks(
      std::vector<ash::NetworkIconInfo>* list) OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    std::set<const Network*> added;

    // Add connected/connecting vpn first.
    if (crosnet->virtual_network()
        && crosnet->virtual_network()->connecting_or_connected()) {
      AddNetworkToList(list, &added, crosnet->virtual_network());
    }

    // VPN (only if logged in).
    if (GetUserLoginStatus() != ash::user::LOGGED_IN_NONE &&
        (crosnet->connected_network() ||
         crosnet->virtual_network_connected())) {
      const VirtualNetworkVector& vpns = crosnet->virtual_networks();
      for (size_t i = 0; i < vpns.size(); ++i)
        AddNetworkToList(list, &added, vpns[i]);
    }
  }

  virtual void GetNetworkAddresses(std::string* ip_address,
                                   std::string* ethernet_mac_address,
                                   std::string* wifi_mac_address) OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    if (crosnet->Connected())
      *ip_address = crosnet->IPAddress();
    else
      *ip_address = std::string();

    *ethernet_mac_address = std::string();
    const NetworkDevice* ether = crosnet->FindEthernetDevice();
    if (ether)
      crosnet->GetIPConfigsAndBlock(ether->device_path(), ethernet_mac_address,
          NetworkLibrary::FORMAT_COLON_SEPARATED_HEX);

    *wifi_mac_address = std::string();
    const NetworkDevice* wifi = crosnet->wifi_enabled() ?
        crosnet->FindWifiDevice() : NULL;
    if (wifi)
      crosnet->GetIPConfigsAndBlock(wifi->device_path(), wifi_mac_address,
          NetworkLibrary::FORMAT_COLON_SEPARATED_HEX);
  }

  virtual void ConnectToNetwork(const std::string& network_id) OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    Network* network = crosnet->FindNetworkByPath(network_id);
    if (network)
      network_menu_->ConnectToNetwork(network);  // Shows settings if connected
    else
      ShowNetworkSettings();
  }

  virtual void RequestNetworkScan() OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    crosnet->RequestNetworkScan();
  }

  virtual void AddBluetoothDevice() OVERRIDE {
    // Open the Bluetooth device dialog, which automatically starts the
    // discovery process.
    content::RecordAction(
        content::UserMetricsAction("OpenAddBluetoothDeviceDialog"));
    chrome::ShowSettingsSubPage(GetAppropriateBrowser(),
                                chrome::kBluetoothAddDeviceSubPage);
  }

  virtual void ToggleAirplaneMode() OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    crosnet->EnableOfflineMode(!crosnet->offline_mode());
  }

  virtual void ToggleWifi() OVERRIDE {
    GetSystemTrayNotifier()->NotifyWillToggleWifi();
    network_menu_->ToggleWifi();
  }

  virtual void ToggleMobile() OVERRIDE {
    network_menu_->ToggleMobile();
  }

  virtual void ToggleBluetooth() OVERRIDE {
    bluetooth_adapter_->SetPowered(!bluetooth_adapter_->IsPowered(),
                                   base::Bind(&base::DoNothing),
                                   base::Bind(&BluetoothPowerFailure));
  }

  virtual void ShowOtherWifi() OVERRIDE {
    network_menu_->ShowOtherWifi();
  }

  virtual void ShowOtherVPN() OVERRIDE {
    network_menu_->ShowOtherVPN();
  }

  virtual void ShowOtherCellular() OVERRIDE {
    network_menu_->ShowOtherCellular();
  }

  virtual bool IsNetworkConnected() OVERRIDE {
    return CrosLibrary::Get()->GetNetworkLibrary()->Connected();
  }

  virtual bool GetWifiAvailable() OVERRIDE {
    return CrosLibrary::Get()->GetNetworkLibrary()->wifi_available();
  }

  virtual bool GetMobileAvailable() OVERRIDE {
    return CrosLibrary::Get()->GetNetworkLibrary()->mobile_available();
  }

  virtual bool GetBluetoothAvailable() OVERRIDE {
    return bluetooth_adapter_->IsPresent();
  }

  virtual bool GetWifiEnabled() OVERRIDE {
    return CrosLibrary::Get()->GetNetworkLibrary()->wifi_enabled();
  }

  virtual bool GetMobileEnabled() OVERRIDE {
    return CrosLibrary::Get()->GetNetworkLibrary()->mobile_enabled();
  }

  virtual bool GetBluetoothEnabled() OVERRIDE {
    return bluetooth_adapter_->IsPowered();
  }

  virtual bool GetMobileScanSupported() OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    const NetworkDevice* mobile = crosnet->FindMobileDevice();
    return mobile ? mobile->support_network_scan() : false;
  }

  virtual bool GetCellularCarrierInfo(std::string* carrier_id,
                                      std::string* topup_url,
                                      std::string* setup_url) OVERRIDE {
    bool result = false;
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    const NetworkDevice* cellular = crosnet->FindCellularDevice();
    if (!cellular)
      return false;

    MobileConfig* config = MobileConfig::GetInstance();
    if (config->IsReady()) {
      *carrier_id = crosnet->GetCellularHomeCarrierId();
      const MobileConfig::Carrier* carrier = config->GetCarrier(*carrier_id);
      if (carrier) {
        *topup_url = carrier->top_up_url();
        result = true;
      }
      const MobileConfig::LocaleConfig* locale_config =
          config->GetLocaleConfig();
      if (locale_config) {
        // Only link to setup URL if SIM card is not inserted.
        if (cellular->is_sim_absent()) {
          *setup_url = locale_config->setup_url();
          result = true;
        }
      }
    }
    return result;
  }

  virtual bool GetWifiScanning() OVERRIDE {
    return CrosLibrary::Get()->GetNetworkLibrary()->wifi_scanning();
  }

  virtual bool GetCellularInitializing() OVERRIDE {
    return CrosLibrary::Get()->GetNetworkLibrary()->cellular_initializing();
  }

  virtual void ShowCellularURL(const std::string& url) OVERRIDE {
    chrome::ShowSingletonTab(GetAppropriateBrowser(), GURL(url));
  }

  virtual void ChangeProxySettings() OVERRIDE {
    CHECK(GetUserLoginStatus() == ash::user::LOGGED_IN_NONE);
    BaseLoginDisplayHost::default_host()->OpenProxySettings();
  }

  virtual ash::VolumeControlDelegate*
  GetVolumeControlDelegate() const OVERRIDE {
    return volume_control_delegate_.get();
  }

  virtual void SetVolumeControlDelegate(
      scoped_ptr<ash::VolumeControlDelegate> delegate) OVERRIDE {
    volume_control_delegate_.swap(delegate);
  }

  virtual base::Time GetSessionStartTime() OVERRIDE {
    return session_start_time_;
  }

  virtual base::TimeDelta GetSessionLengthLimit() OVERRIDE {
    return session_length_limit_;
  }

  virtual int GetSystemTrayMenuWidth() OVERRIDE {
    return l10n_util::GetLocalizedContentsWidthInPixels(
        IDS_SYSTEM_TRAY_MENU_BUBBLE_WIDTH_PIXELS);
  }

 private:
  ash::SystemTray* GetPrimarySystemTray() {
    return ash::Shell::GetInstance()->GetPrimarySystemTray();
  }

  ash::SystemTrayNotifier* GetSystemTrayNotifier() {
    return ash::Shell::GetInstance()->system_tray_notifier();
  }

  // Returns the last active browser. If there is no such browser, creates a new
  // browser window with an empty tab and returns it.
  Browser* GetAppropriateBrowser() {
    return chrome::FindOrCreateTabbedBrowser(
        ProfileManager::GetDefaultProfileOrOffTheRecord(),
        chrome::HOST_DESKTOP_TYPE_ASH);
  }

  void SetProfile(Profile* profile) {
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
        prefs::kShouldAlwaysShowAccessibilityMenu,
        base::Bind(&SystemTrayDelegate::OnAccessibilityModeChanged,
                   base::Unretained(this),
                   ash::A11Y_NOTIFICATION_NONE));

    UpdateClockType();
    UpdateShowLogoutButtonInTray();
    search_key_mapped_to_ =
        profile->GetPrefs()->GetInteger(prefs::kLanguageRemapSearchKeyTo);
  }

  void ObserveGDataUpdates() {
    DriveSystemService* system_service = FindDriveSystemService();
    if (!system_service)
      return;

    system_service->drive_service()->AddObserver(this);
  }

  void UpdateClockType() {
    clock_type_ =
        user_pref_registrar_->prefs()->GetBoolean(prefs::kUse24HourClock) ?
            base::k24HourClock : base::k12HourClock;
    GetSystemTrayNotifier()->NotifyDateFormatChanged();
  }

  void UpdateShowLogoutButtonInTray() {
    GetSystemTrayNotifier()->NotifyShowLoginButtonChanged(
        user_pref_registrar_->prefs()->GetBoolean(
            prefs::kShowLogoutButtonInTray));
  }

  void UpdateSessionStartTime() {
    session_start_time_ = base::Time::FromInternalValue(
        local_state_registrar_.prefs()->GetInt64(prefs::kSessionStartTime));
    GetSystemTrayNotifier()->NotifySessionStartTimeChanged(session_start_time_);
  }

  void UpdateSessionLengthLimit() {
    const PrefService::Preference* session_length_limit_pref =
        local_state_registrar_.prefs()->
            FindPreference(prefs::kSessionLengthLimit);
    int limit;
    if (session_length_limit_pref->IsDefaultValue() ||
        !session_length_limit_pref->GetValue()->GetAsInteger(&limit)) {
      session_length_limit_ = base::TimeDelta();
    } else {
      session_length_limit_ = base::TimeDelta::FromMilliseconds(
          std::min(std::max(limit, kSessionLengthLimitMinMs),
              kSessionLengthLimitMaxMs));
    }
    GetSystemTrayNotifier()->NotifySessionLengthLimitChanged(
        session_length_limit_);
  }

  void NotifyRefreshNetwork() {
    chromeos::NetworkLibrary* crosnet =
        chromeos::CrosLibrary::Get()->GetNetworkLibrary();
    const Network* network = crosnet->connected_network();
    ash::NetworkIconInfo info;
    if (network)
      info = CreateNetworkIconInfo(network);
    info.image = network_icon_->GetIconAndText(&info.description);
    info.tray_icon_visible = network_icon_->ShouldShowIconInTray();
    GetSystemTrayNotifier()->NotifyRefreshNetwork(info);
    GetSystemTrayNotifier()->NotifyVpnRefreshNetwork(info);
  }

  void RefreshNetworkObserver(NetworkLibrary* crosnet) {
    const Network* network = crosnet->active_nonvirtual_network();
    std::string new_path = network ? network->service_path() : std::string();
    if (active_network_path_ != new_path) {
      if (!active_network_path_.empty())
        crosnet->RemoveNetworkObserver(active_network_path_, this);
      if (!new_path.empty())
        crosnet->AddNetworkObserver(new_path, this);
      active_network_path_ = new_path;
    }
  }

  void AddNetworkToList(std::vector<ash::NetworkIconInfo>* list,
                        std::set<const Network*>* added,
                        const Network* network) {
    // Only add networks to the list once.
    if (added->find(network) != added->end())
      return;

    ash::NetworkIconInfo info = CreateNetworkIconInfo(network);
    switch (network->type()) {
      case TYPE_ETHERNET:
        if (info.name.empty()) {
          info.name =
              l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
        }
        break;
      case TYPE_CELLULAR: {
        const CellularNetwork* cellular =
            static_cast<const CellularNetwork*>(network);
        ActivationState state = cellular->activation_state();
        if (state == ACTIVATION_STATE_NOT_ACTIVATED ||
            state == ACTIVATION_STATE_PARTIALLY_ACTIVATED) {
          // If a cellular network needs to be activated,
          // then do not show it in the lock screen.
          if (GetUserLoginStatus() == ash::user::LOGGED_IN_LOCKED)
            return;

          info.description = l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_NETWORK_LIST_ACTIVATE, info.name);
        } else if (state == ACTIVATION_STATE_ACTIVATING) {
          info.description = l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_NETWORK_LIST_ACTIVATING, info.name);
        } else if (network->connecting()) {
          info.description = l10n_util::GetStringFUTF16(
              IDS_STATUSBAR_NETWORK_DEVICE_STATUS,
              info.name, l10n_util::GetStringUTF16(
                  IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING));
        }
        break;
      }
      case TYPE_VPN:
      case TYPE_WIFI:
      case TYPE_WIMAX:
      case TYPE_BLUETOOTH:
      case TYPE_UNKNOWN:
        break;
    }
    if (network->connecting()) {
      info.description = l10n_util::GetStringFUTF16(
          IDS_STATUSBAR_NETWORK_DEVICE_STATUS,
          info.name,
          l10n_util::GetStringUTF16(
              IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING));
    }
    added->insert(network);
    list->push_back(info);
  }

  // Overridden from AudioHandler::VolumeObserver.
  virtual void OnVolumeChanged() OVERRIDE {
    float level = AudioHandler::GetInstance()->GetVolumePercent() / 100.f;
    GetSystemTrayNotifier()->NotifyVolumeChanged(level);
  }

  // Overridden from AudioHandler::VolumeObserver.
  virtual void OnMuteToggled() OVERRIDE {
    GetSystemTrayNotifier()->NotifyMuteToggled();
  }

  // Overridden from PowerManagerClient::Observer.
  virtual void BrightnessChanged(int level, bool user_initiated) OVERRIDE {
    double leveld = static_cast<double>(level);
    GetSystemTrayNotifier()->NotifyBrightnessChanged(leveld, user_initiated);
  }

  virtual void PowerChanged(const PowerSupplyStatus& power_status) OVERRIDE {
    power_supply_status_ = power_status;
    GetSystemTrayNotifier()->NotifyPowerStatusChanged(power_status);
  }

  // Overridden from PowerManagerClient::Observer:
  virtual void SystemResumed(const base::TimeDelta& sleep_duration) OVERRIDE {
    GetSystemTrayNotifier()->NotifyRefreshClock();
  }

  // Overridden from SessionManagerClient::Observer.
  virtual void LockScreen() OVERRIDE {
    screen_locked_ = true;
    ash::Shell::GetInstance()->UpdateAfterLoginStatusChange(
        GetUserLoginStatus());
  }

  virtual void UnlockScreen() OVERRIDE {
    screen_locked_ = false;
    ash::Shell::GetInstance()->UpdateAfterLoginStatusChange(
        GetUserLoginStatus());
  }

  // TODO(sad): Override more from PowerManagerClient::Observer here.

  // Overridden from NetworkMenuIcon::Delegate.
  virtual void NetworkMenuIconChanged() OVERRIDE {
    NotifyRefreshNetwork();
  }

  // Overridden from NetworkMenu::Delegate.
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE {
    return GetNativeWindowByStatus(GetUserLoginStatus());
  }

  virtual void OpenButtonOptions() OVERRIDE {
  }

  virtual bool ShouldOpenButtonOptions() const OVERRIDE {
    return false;
  }

  // Overridden from NetworkLibrary::NetworkManagerObserver.
  virtual void OnNetworkManagerChanged(NetworkLibrary* crosnet) OVERRIDE {
    RefreshNetworkObserver(crosnet);
    data_promo_notification_->ShowOptionalMobileDataPromoNotification(
        crosnet, GetPrimarySystemTray(), this);
    UpdateCellular();

    NotifyRefreshNetwork();
  }

  // Overridden from NetworkLibrary::NetworkObserver.
  virtual void OnNetworkChanged(NetworkLibrary* crosnet,
      const Network* network) OVERRIDE {
    NotifyRefreshNetwork();
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
      case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED: {
        // GData system service exists by the time if enabled.
        ObserveGDataUpdates();
        break;
      }
      case chrome::NOTIFICATION_PROFILE_CREATED: {
        SetProfile(content::Source<Profile>(source).ptr());
        registrar_.Remove(this,
                          chrome::NOTIFICATION_PROFILE_CREATED,
                          content::NotificationService::AllSources());
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
        accessibility::AccessibilityStatusEventDetails* accessibility_status =
            content::Details<accessibility::AccessibilityStatusEventDetails>(
                details).ptr();
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

  // Overridden from InputMethodManager::Observer.
  virtual void InputMethodChanged(
      input_method::InputMethodManager* manager, bool show_message) OVERRIDE {
    GetSystemTrayNotifier()->NotifyRefreshIME(show_message);
  }

  virtual void InputMethodPropertyChanged(
      input_method::InputMethodManager* manager) OVERRIDE {
    GetSystemTrayNotifier()->NotifyRefreshIME(false);
  }

  // google_apis::DriveServiceObserver overrides.
  virtual void OnProgressUpdate(
      const google_apis::OperationProgressStatusList& list) OVERRIDE {
    std::vector<ash::DriveOperationStatus> ui_list = GetDriveStatusList(list);
    GetSystemTrayNotifier()->NotifyRefreshDrive(ui_list);

    // If we have something to report right now (i.e. completion status only),
    // we need to delayed re-check the status in few seconds to ensure we
    // raise events that will let us properly clear the uber tray state.
    if (list.size() > 0) {
      bool has_in_progress_items = false;
      for (google_apis::OperationProgressStatusList::const_iterator it =
               list.begin();
           it != list.end(); ++it) {
        if (it->transfer_state == google_apis::OPERATION_STARTED ||
            it->transfer_state == google_apis::OPERATION_IN_PROGRESS ||
            it->transfer_state == google_apis::OPERATION_SUSPENDED) {
          has_in_progress_items = true;
          break;
        }
      }

      if (!has_in_progress_items) {
        content::BrowserThread::PostDelayedTask(
            content::BrowserThread::UI,
            FROM_HERE,
            base::Bind(&SystemTrayDelegate::RecheckGDataOperations,
                       ui_weak_ptr_factory_->GetWeakPtr()),
            base::TimeDelta::FromMilliseconds(kGDataOperationRecheckDelayMs));
      }
    }
  }

  // Pulls the list of ongoing drive operations and initiates status update.
  // This method is needed to ensure delayed cleanup of the latest reported
  // status in UI in cases when there are no new changes coming (i.e. when the
  // last set of transfer operations completed).
  void RecheckGDataOperations() {
    DriveSystemService* system_service = FindDriveSystemService();
    if (!system_service)
      return;

    OnProgressUpdate(system_service->drive_service()->GetProgressStatusList());
  }

  DriveSystemService* FindDriveSystemService() {
    Profile* profile = ProfileManager::GetDefaultProfile();
    return DriveSystemServiceFactory::FindForProfile(profile);
  }

  // Overridden from system::TimezoneSettings::Observer.
  virtual void TimezoneChanged(const icu::TimeZone& timezone) OVERRIDE {
    GetSystemTrayNotifier()->NotifyRefreshClock();
  }

  // Overridden from SystemClockClient::Observer.
  virtual void SystemClockUpdated() OVERRIDE {
    GetSystemTrayNotifier()->NotifySystemClockTimeUpdated();
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
    if (!base::chromeos::IsRunningOnChromeOS() ||
        search_key_mapped_to_ == input_method::kCapsLockKey)
      search_mapped_to_caps_lock = true;
    GetSystemTrayNotifier()->NotifyCapsLockChanged(
        enabled, search_mapped_to_caps_lock);
  }

  // Overridden from ash::NetworkTrayDelegate
  virtual void NotificationLinkClicked(
      ash::NetworkObserver::MessageType message_type,
      size_t link_index) OVERRIDE {
    if (message_type == ash::NetworkObserver::ERROR_OUT_OF_CREDITS) {
      const CellularNetwork* cellular =
          CrosLibrary::Get()->GetNetworkLibrary()->cellular_network();
      if (cellular)
        ConnectToNetwork(cellular->service_path());
      ash::Shell::GetInstance()->system_tray_notifier()->
          NotifyClearNetworkMessage(message_type);
    }
    if (message_type != ash::NetworkObserver::MESSAGE_DATA_PROMO)
      return;
    // If we have deal info URL defined that means that there're
    // 2 links in bubble. Let the user close it manually then thus giving
    // ability to navigate to second link.
    // mobile_data_bubble_ will be set to NULL in BubbleClosing callback.
    std::string deal_info_url = data_promo_notification_->deal_info_url();
    std::string deal_topup_url = data_promo_notification_->deal_topup_url();
    if (deal_info_url.empty())
      data_promo_notification_->CloseNotification();

    std::string deal_url_to_open;
    if (link_index == 0) {
      if (!deal_topup_url.empty()) {
        deal_url_to_open = deal_topup_url;
      } else {
        const Network* cellular =
            CrosLibrary::Get()->GetNetworkLibrary()->cellular_network();
        if (!cellular)
          return;
        network_menu_->ShowTabbedNetworkSettings(cellular);
        return;
      }
    } else if (link_index == 1) {
      deal_url_to_open = deal_info_url;
    }

    if (!deal_url_to_open.empty()) {
      Browser* browser = GetAppropriateBrowser();
      if (!browser)
        return;
      chrome::ShowSingletonTab(browser, GURL(deal_url_to_open));
    }
  }

  virtual void UpdateEnterpriseDomain() {
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

  void UpdateCellular() {
    const CellularNetworkVector& cellular_networks =
        CrosLibrary::Get()->GetNetworkLibrary()->cellular_networks();
    if (cellular_networks.empty())
      return;
    // We only care about the first cellular network (in practice there will
    // only ever be one)
    const CellularNetwork* cellular = cellular_networks[0];
    if (cellular->activation_state() == ACTIVATION_STATE_ACTIVATING) {
      cellular_activating_ = true;
    } else if (cellular->activated() && cellular_activating_) {
      cellular_activating_ = false;
      ash::NetworkObserver::NetworkType type = NetworkTypeForCellular(cellular);
      ash::Shell::GetInstance()->system_tray_notifier()->
          NotifySetNetworkMessage(
              NULL,
              ash::NetworkObserver::MESSAGE_DATA_PROMO,
              type,
              l10n_util::GetStringUTF16(
                  IDS_NETWORK_CELLULAR_ACTIVATED_TITLE),
              l10n_util::GetStringFUTF16(
                  IDS_NETWORK_CELLULAR_ACTIVATED,
                  UTF8ToUTF16((cellular->name()))),
                  std::vector<string16>());
    }
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
            ash::switches::kAshDisableNewNetworkStatusArea)) {
      return;
    }
    // Trigger "Out of credits" notification (for NetworkLibrary impl)
    if (cellular->out_of_credits() && !cellular_out_of_credits_) {
      cellular_out_of_credits_ = true;
      ash::NetworkObserver::NetworkType type = NetworkTypeForCellular(cellular);
      std::vector<string16> links;
      links.push_back(
          l10n_util::GetStringFUTF16(IDS_NETWORK_OUT_OF_CREDITS_LINK,
                                     UTF8ToUTF16(cellular->name())));
      ash::Shell::GetInstance()->system_tray_notifier()->
          NotifySetNetworkMessage(
              this, ash::NetworkObserver::ERROR_OUT_OF_CREDITS, type,
              l10n_util::GetStringUTF16(IDS_NETWORK_OUT_OF_CREDITS_TITLE),
              l10n_util::GetStringUTF16(IDS_NETWORK_OUT_OF_CREDITS_BODY),
              links);
    } else if (!cellular->out_of_credits() && cellular_out_of_credits_) {
      cellular_out_of_credits_ = false;
    }
  }

  scoped_ptr<base::WeakPtrFactory<SystemTrayDelegate> > ui_weak_ptr_factory_;
  scoped_ptr<NetworkMenuIcon> network_icon_;
  scoped_ptr<NetworkMenuIcon> network_icon_dark_;
  scoped_ptr<NetworkMenuIcon> network_icon_vpn_;
  scoped_ptr<NetworkMenu> network_menu_;
  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar local_state_registrar_;
  scoped_ptr<PrefChangeRegistrar> user_pref_registrar_;
  std::string active_network_path_;
  PowerSupplyStatus power_supply_status_;
  base::HourClockType clock_type_;
  int search_key_mapped_to_;
  bool screen_locked_;
  base::Time session_start_time_;
  base::TimeDelta session_length_limit_;
  std::string enterprise_domain_;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  scoped_ptr<DataPromoNotification> data_promo_notification_;
  bool cellular_activating_;
  bool cellular_out_of_credits_;

  scoped_ptr<ash::VolumeControlDelegate> volume_control_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegate);
};

}  // namespace

ash::SystemTrayDelegate* CreateSystemTrayDelegate() {
  return new chromeos::SystemTrayDelegate();
}

}  // namespace chromeos
