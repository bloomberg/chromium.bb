// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/ash_system_tray_delegate.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/audio/audio_observer.h"
#include "ash/system/bluetooth/bluetooth_observer.h"
#include "ash/system/brightness/brightness_observer.h"
#include "ash/system/date/clock_observer.h"
#include "ash/system/drive/drive_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/network/network_observer.h"
#include "ash/system/power/power_status_observer.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/tray_caps_lock.h"
#include "ash/system/user/update_observer.h"
#include "ash/system/user/user_observer.h"
#include "ash/volume_control_delegate.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/chromeos/chromeos_version.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/audio/audio_handler.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_device.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/gdata/drive_service_interface.h"
#include "chrome/browser/chromeos/gdata/drive_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/status/data_promo_notification.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/chromeos/status/network_menu_icon.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chrome/browser/chromeos/system_key_event_listener.h"
#include "chrome/browser/google_apis/operation_registry.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/volume_controller_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using gdata::DriveSystemService;
using gdata::DriveSystemServiceFactory;
using gdata::OperationRegistry;

namespace chromeos {

namespace {

// Time delay for rechecking gdata operation when we suspect that there will
// be no upcoming activity notifications that need to be pushed to UI.
const int kGDataOperationRecheckDelayMs = 5000;

ash::NetworkIconInfo CreateNetworkIconInfo(const Network* network,
                                           NetworkMenu* network_menu) {
  ash::NetworkIconInfo info;
  info.name = UTF8ToUTF16(network->name());
  info.image = NetworkMenuIcon::GetImage(network, NetworkMenuIcon::COLOR_DARK);
  info.service_path = network->service_path();
  info.highlight = network->connected() || network->connecting();
  return info;
}

void ExtractIMEInfo(const input_method::InputMethodDescriptor& ime,
                    const input_method::InputMethodUtil& util,
                    ash::IMEInfo* info) {
  info->id = ime.id();
  info->name = util.GetInputMethodLongName(ime);
  info->short_name = util.GetInputMethodShortName(ime);
  info->third_party = ime.third_party();
}

ash::DriveOperationStatusList GetDriveStatusList(
    const std::vector<OperationRegistry::ProgressStatus>& list) {
  ash::DriveOperationStatusList results;
  for (OperationRegistry::ProgressStatusList::const_iterator it =
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

void BluetoothDeviceConnectError() {
  // TODO(sad): Do something?
}

class SystemTrayDelegate : public ash::SystemTrayDelegate,
                           public AudioHandler::VolumeObserver,
                           public PowerManagerClient::Observer,
                           public SessionManagerClient::Observer,
                           public NetworkMenuIcon::Delegate,
                           public NetworkMenu::Delegate,
                           public NetworkLibrary::NetworkManagerObserver,
                           public NetworkLibrary::NetworkObserver,
                           public NetworkLibrary::CellularDataPlanObserver,
                           public gdata::OperationRegistry::Observer,
                           public content::NotificationObserver,
                           public input_method::InputMethodManager::Observer,
                           public system::TimezoneSettings::Observer,
                           public BluetoothAdapter::Observer,
                           public SystemKeyEventListener::CapsLockObserver,
                           public ash::NetworkTrayDelegate {
 public:
  explicit SystemTrayDelegate(ash::SystemTray* tray)
      : tray_(tray),
        ui_weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(
            new base::WeakPtrFactory<SystemTrayDelegate>(this))),
        network_icon_(ALLOW_THIS_IN_INITIALIZER_LIST(
                      new NetworkMenuIcon(this, NetworkMenuIcon::MENU_MODE))),
        network_icon_dark_(ALLOW_THIS_IN_INITIALIZER_LIST(
                      new NetworkMenuIcon(this, NetworkMenuIcon::MENU_MODE))),
        network_menu_(ALLOW_THIS_IN_INITIALIZER_LIST(new NetworkMenu(this))),
        clock_type_(base::k24HourClock),
        search_key_mapped_to_(input_method::kSearchKey),
        screen_locked_(false),
        connected_network_state_(STATE_UNKNOWN),
        data_promo_notification_(new DataPromoNotification()),
        volume_control_delegate_(ALLOW_THIS_IN_INITIALIZER_LIST(
            new VolumeController)) {
    AudioHandler::GetInstance()->AddVolumeObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestStatusUpdate(
        PowerManagerClient::UPDATE_INITIAL);
    DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(this);

    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    crosnet->AddNetworkManagerObserver(this);
    OnNetworkManagerChanged(crosnet);
    crosnet->AddCellularDataPlanObserver(this);

    input_method::InputMethodManager::GetInstance()->AddObserver(this);

    system::TimezoneSettings::GetInstance()->AddObserver(this);

    if (SystemKeyEventListener::GetInstance())
      SystemKeyEventListener::GetInstance()->AddCapsLockObserver(this);

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

    accessibility_enabled_.Init(prefs::kSpokenFeedbackEnabled,
                                g_browser_process->local_state(), this);

    network_icon_->SetResourceColorTheme(NetworkMenuIcon::COLOR_LIGHT);
    network_icon_dark_->SetResourceColorTheme(NetworkMenuIcon::COLOR_DARK);

    bluetooth_adapter_ = BluetoothAdapter::DefaultAdapter();
    bluetooth_adapter_->AddObserver(this);
  }

  virtual ~SystemTrayDelegate() {
    AudioHandler* audiohandler = AudioHandler::GetInstance();
    if (audiohandler)
      audiohandler->RemoveVolumeObserver(this);
    DBusThreadManager::Get()->GetSessionManagerClient()->RemoveObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    if (crosnet) {
      crosnet->RemoveNetworkManagerObserver(this);
      crosnet->RemoveCellularDataPlanObserver(this);
    }
    input_method::InputMethodManager::GetInstance()->RemoveObserver(this);
    system::TimezoneSettings::GetInstance()->RemoveObserver(this);
    if (SystemKeyEventListener::GetInstance())
      SystemKeyEventListener::GetInstance()->RemoveCapsLockObserver(this);
    bluetooth_adapter_->RemoveObserver(this);

    // Stop observing gdata operations.
    DriveSystemService* system_service = FindDriveSystemService();
    if (system_service) {
      system_service->drive_service()->operation_registry()->
          RemoveObserver(this);
    }
  }

  // Overridden from ash::SystemTrayDelegate:
  virtual bool GetTrayVisibilityOnStartup() OVERRIDE {
    // In case of OOBE / sign in screen tray will be shown later.
    return UserManager::Get()->IsUserLoggedIn();
  }

  virtual const string16 GetUserDisplayName() const OVERRIDE {
    return UserManager::Get()->GetLoggedInUser().GetDisplayName();
  }

  virtual const std::string GetUserEmail() const OVERRIDE {
    return UserManager::Get()->GetLoggedInUser().display_email();
  }

  virtual const gfx::ImageSkia& GetUserImage() const OVERRIDE {
    return UserManager::Get()->GetLoggedInUser().image();
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
      return ash::user::LOGGED_IN_KIOSK;
    return ash::user::LOGGED_IN_USER;
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

  virtual void ShutDown() OVERRIDE {
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestShutdown();
    if (!base::chromeos::IsRunningOnChromeOS())
      browser::AttemptUserExit();
  }

  virtual void SignOut() OVERRIDE {
    browser::AttemptUserExit();
  }

  virtual void RequestLockScreen() OVERRIDE {
    DBusThreadManager::Get()->GetSessionManagerClient()->RequestLockScreen();
  }

  virtual void RequestRestart() OVERRIDE {
    DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
  }

  virtual void GetAvailableBluetoothDevices(
      ash::BluetoothDeviceList* list) OVERRIDE {
    BluetoothAdapter::DeviceList devices = bluetooth_adapter_->GetDevices();
    for (size_t i = 0; i < devices.size(); ++i) {
      BluetoothDevice* device = devices[i];
      if (!device->IsPaired())
        continue;
      ash::BluetoothDeviceInfo info;
      info.address = device->address();
      info.display_name = device->GetName();
      info.connected = device->IsConnected();
      list->push_back(info);
    }
  }

  virtual void ToggleBluetoothConnection(const std::string& address) OVERRIDE {
    BluetoothDevice* device = bluetooth_adapter_->GetDevice(address);
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
    }
  }

  virtual void GetCurrentIME(ash::IMEInfo* info) OVERRIDE {
    input_method::InputMethodManager* manager =
        input_method::InputMethodManager::GetInstance();
    input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
    input_method::InputMethodDescriptor ime = manager->GetCurrentInputMethod();
    ExtractIMEInfo(ime, *util, info);
    info->selected = true;
  }

  virtual void GetAvailableIMEList(ash::IMEInfoList* list) OVERRIDE {
    input_method::InputMethodManager* manager =
        input_method::InputMethodManager::GetInstance();
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
        input_method::InputMethodManager::GetInstance();
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
    input_method::InputMethodManager::GetInstance()->ChangeInputMethod(ime_id);
  }

  virtual void ActivateIMEProperty(const std::string& key) OVERRIDE {
    input_method::InputMethodManager::GetInstance()->
        ActivateInputMethodProperty(key);
  }

  virtual void CancelDriveOperation(const FilePath& file_path) OVERRIDE {
    DriveSystemService* system_service = FindDriveSystemService();
    if (!system_service)
      return;

    system_service->drive_service()->operation_registry()->CancelForFilePath(
        file_path);
  }

  virtual void GetDriveOperationStatusList(
      ash::DriveOperationStatusList* list) OVERRIDE {
    DriveSystemService* system_service = FindDriveSystemService();
    if (!system_service)
      return;

    *list = GetDriveStatusList(
        system_service->drive_service()->operation_registry()->
            GetProgressStatusList());
  }


  virtual void GetMostRelevantNetworkIcon(ash::NetworkIconInfo* info,
                                          bool dark) OVERRIDE {
    NetworkMenuIcon* icon =
        dark ? network_icon_dark_.get() : network_icon_.get();
    info->image = icon->GetIconAndText(&info->description);
    info->tray_icon_visible = icon->ShouldShowIconInTray();
  }

  virtual void GetAvailableNetworks(
      std::vector<ash::NetworkIconInfo>* list) OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();

    std::set<const Network*> added;

    // Add the active network first.

    if (crosnet->active_network()) {
      AddNetworkToList(list, &added, crosnet->active_network());
    }

    // Add connected/connecting network(s) second, by type.

    if (crosnet->virtual_network()
        && crosnet->virtual_network()->connecting_or_connected()) {
      AddNetworkToList(list, &added, crosnet->virtual_network());
    }
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

    // VPN (only if logged in).
    if (GetUserLoginStatus() != ash::user::LOGGED_IN_NONE &&
        (crosnet->connected_network() ||
         crosnet->virtual_network_connected())) {
      const VirtualNetworkVector& vpns = crosnet->virtual_networks();
      for (size_t i = 0; i < vpns.size(); ++i)
        AddNetworkToList(list, &added, vpns[i]);
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
      crosnet->GetIPConfigs(ether->device_path(), ethernet_mac_address,
          NetworkLibrary::FORMAT_COLON_SEPARATED_HEX);

    *wifi_mac_address = std::string();
    const NetworkDevice* wifi = crosnet->wifi_enabled() ?
        crosnet->FindWifiDevice() : NULL;
    if (wifi)
      crosnet->GetIPConfigs(wifi->device_path(), wifi_mac_address,
          NetworkLibrary::FORMAT_COLON_SEPARATED_HEX);
  }

  virtual void ConnectToNetwork(const std::string& network_id) OVERRIDE {
    NetworkLibrary* crosnet = CrosLibrary::Get()->GetNetworkLibrary();
    Network* network = crosnet->FindNetworkByPath(network_id);
    if (network)
      network_menu_->ConnectToNetwork(network);
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
    tray_->network_observer()->OnWillToggleWifi();
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

  virtual void ShowCellularURL(const std::string& url) OVERRIDE {
    chrome::ShowSingletonTab(GetAppropriateBrowser(), GURL(url));
  }

  virtual void ChangeProxySettings() OVERRIDE {
    CHECK(GetUserLoginStatus() == ash::user::LOGGED_IN_NONE);
    BaseLoginDisplayHost::default_host()->OpenProxySettings();
  }

  virtual ash::VolumeControlDelegate* GetVolumeControlDelegate() const OVERRIDE
  {
    return volume_control_delegate_.get();
  }

  virtual void SetVolumeControlDelegate(
      scoped_ptr<ash::VolumeControlDelegate> delegate) OVERRIDE {
    volume_control_delegate_.swap(delegate);
  }
 private:
  // Returns the last active browser. If there is no such browser, creates a new
  // browser window with an empty tab and returns it.
  Browser* GetAppropriateBrowser() {
    return browser::FindOrCreateTabbedBrowser(
        ProfileManager::GetDefaultProfileOrOffTheRecord());
  }

  void SetProfile(Profile* profile) {
    pref_registrar_.reset(new PrefChangeRegistrar);
    pref_registrar_->Init(profile->GetPrefs());
    pref_registrar_->Add(prefs::kUse24HourClock, this);
    pref_registrar_->Add(prefs::kLanguageXkbRemapSearchKeyTo, this);
    UpdateClockType(profile->GetPrefs());
    search_key_mapped_to_ =
        profile->GetPrefs()->GetInteger(prefs::kLanguageXkbRemapSearchKeyTo);
  }

  void ObserveGDataUpdates() {
    DriveSystemService* system_service = FindDriveSystemService();
    if (!system_service)
      return;

    system_service->drive_service()->operation_registry()->AddObserver(this);
  }

  void UpdateClockType(PrefService* service) {
    clock_type_ = service->GetBoolean(prefs::kUse24HourClock) ?
        base::k24HourClock : base::k12HourClock;
    ash::ClockObserver* observer = tray_->clock_observer();
    if (observer)
      observer->OnDateFormatChanged();
  }

  void NotifyRefreshClock() {
    ash::ClockObserver* observer = tray_->clock_observer();
    if (observer)
      observer->Refresh();
  }

  void NotifyRefreshNetwork() {
    ash::NetworkObserver* observer = tray_->network_observer();
    chromeos::NetworkLibrary* crosnet =
        chromeos::CrosLibrary::Get()->GetNetworkLibrary();
    if (observer) {
      ash::NetworkIconInfo info;
      info.image = network_icon_->GetIconAndText(&info.description);
      info.tray_icon_visible = network_icon_->ShouldShowIconInTray();
      observer->OnNetworkRefresh(info);
    }

    // Update Accessibility.

    std::string connected_network_path;
    ConnectionState connected_network_state(STATE_UNKNOWN);
    if (crosnet->connected_network()) {
      connected_network_path = crosnet->connected_network()->service_path();
      connected_network_state = crosnet->connected_network()->state();
    }
    if (accessibility::IsSpokenFeedbackEnabled()) {
      bool speak = false;
      if ((connected_network_path_ != connected_network_path) ||
          (Network::IsConnectedState(connected_network_state_) &&
           !Network::IsConnectedState(connected_network_state)) ||
          (Network::IsConnectingState(connected_network_state_) &&
           !Network::IsConnectingState(connected_network_state)) ||
          (Network::IsDisconnectedState(connected_network_state_) &&
           !Network::IsDisconnectedState(connected_network_state))) {
        speak = true;
      }

      if (speak)
        AccessibilitySpeak(crosnet->connected_network());
    }

    connected_network_path_ = connected_network_path_;
    connected_network_state_ = connected_network_state;
  }

  void NotifyRefreshBluetooth() {
    ash::BluetoothObserver* observer = tray_->bluetooth_observer();
    if (observer)
      observer->OnBluetoothRefresh();
  }

  void NotifyRefreshIME(bool show_message) {
    ash::IMEObserver* observer = tray_->ime_observer();
    if (observer)
      observer->OnIMERefresh(show_message);
  }

  void NotifyRefreshDrive(ash::DriveOperationStatusList& list) {
    ash::DriveObserver* observer = tray_->drive_observer();
    if (observer)
      observer->OnDriveRefresh(list);
  }

  void RefreshNetworkObserver(NetworkLibrary* crosnet) {
    const Network* network = crosnet->active_network();
    std::string new_path = network ? network->service_path() : std::string();
    if (active_network_path_ != new_path) {
      if (!active_network_path_.empty())
        crosnet->RemoveNetworkObserver(active_network_path_, this);
      if (!new_path.empty())
        crosnet->AddNetworkObserver(new_path, this);
      active_network_path_ = new_path;
    }
  }

  void RefreshNetworkDeviceObserver(NetworkLibrary* crosnet) {
    const NetworkDevice* cellular = crosnet->FindCellularDevice();
    std::string new_cellular_device_path = cellular ?
        cellular->device_path() : std::string();
    if (cellular_device_path_ != new_cellular_device_path)
      cellular_device_path_ = new_cellular_device_path;
  }

  // Generate accessability text and call Speak().
  void AccessibilitySpeak(const Network* network) {
    if (!network)
      return;
    NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
    std::string connection_string;
    if (Network::IsConnectedState(network->state())) {
      switch (network->type()) {
        case TYPE_ETHERNET:
          connection_string = l10n_util::GetStringFUTF8(
              IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
              l10n_util::GetStringUTF16(
                  IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET));
          break;
        default:
          connection_string = l10n_util::GetStringFUTF8(
              IDS_STATUSBAR_NETWORK_CONNECTED_TOOLTIP,
              UTF8ToUTF16(network->name()));
          break;
      }
    } else if (Network::IsConnectingState(network->state())) {
      const Network* connecting_network = cros->connecting_network();
      if (connecting_network && connecting_network->type() != TYPE_ETHERNET) {
        connection_string = l10n_util::GetStringFUTF8(
            IDS_STATUSBAR_NETWORK_CONNECTING_TOOLTIP,
            UTF8ToUTF16(connecting_network->name()));
      }
    } else if (Network::IsDisconnectedState(network->state())) {
      connection_string = l10n_util::GetStringUTF8(
          IDS_STATUSBAR_NETWORK_NO_NETWORK_TOOLTIP);
    }
    accessibility::Speak(connection_string.c_str());
  }

  void AddNetworkToList(std::vector<ash::NetworkIconInfo>* list,
                        std::set<const Network*>* added,
                        const Network* network) {
    // Only add networks to the list once.
    if (added->find(network) != added->end())
      return;

    ash::NetworkIconInfo info = CreateNetworkIconInfo(network,
                                                      network_menu_.get());
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
              IDS_STATUSBAR_NETWORK_DEVICE_ACTIVATE,
              info.name);
        } else if (state == ACTIVATION_STATE_ACTIVATING) {
          info.description = l10n_util::GetStringFUTF16(
              IDS_STATUSBAR_NETWORK_DEVICE_STATUS,
              info.name, l10n_util::GetStringUTF16(
                  IDS_STATUSBAR_NETWORK_DEVICE_ACTIVATING));
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
    tray_->audio_observer()->OnVolumeChanged(level);
  }

  // Overridden from AudioHandler::VolumeObserver.
  virtual void OnMuteToggled() OVERRIDE {
    tray_->audio_observer()->OnMuteToggled();
  }

  // Overridden from PowerManagerClient::Observer.
  virtual void BrightnessChanged(int level, bool user_initiated) OVERRIDE {
    tray_->brightness_observer()->
        OnBrightnessChanged(static_cast<double>(level), user_initiated);
  }

  virtual void PowerChanged(const PowerSupplyStatus& power_status) OVERRIDE {
    power_supply_status_ = power_status;
    FOR_EACH_OBSERVER(ash::PowerStatusObserver, tray_->power_status_observers(),
        OnPowerStatusChanged(power_status));
  }

  virtual void SystemResumed() OVERRIDE {
    NotifyRefreshClock();
  }

  // Overridden from SessionManagerClient::Observer.
  virtual void LockScreen() OVERRIDE {
    screen_locked_ = true;
    ash::Shell::GetInstance()->status_area_widget()->
        UpdateAfterLoginStatusChange(GetUserLoginStatus());
  }

  virtual void UnlockScreen() OVERRIDE {
    screen_locked_ = false;
    ash::Shell::GetInstance()->status_area_widget()->
        UpdateAfterLoginStatusChange(GetUserLoginStatus());
  }

  // TODO(sad): Override more from PowerManagerClient::Observer here (e.g.
  // PowerButtonStateChanged etc.).

  // Overridden from NetworkMenuIcon::Delegate.
  virtual void NetworkMenuIconChanged() OVERRIDE {
    NotifyRefreshNetwork();
  }

  // Overridden from NetworkMenu::Delegate.
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE {
    return ash::Shell::GetContainer(
        ash::Shell::GetPrimaryRootWindow(),
        GetUserLoginStatus() == ash::user::LOGGED_IN_NONE ?
            ash::internal::kShellWindowId_LockSystemModalContainer :
            ash::internal::kShellWindowId_SystemModalContainer);
  }

  virtual void OpenButtonOptions() OVERRIDE {
  }

  virtual bool ShouldOpenButtonOptions() const OVERRIDE {
    return false;
  }

  // Overridden from NetworkLibrary::NetworkManagerObserver.
  virtual void OnNetworkManagerChanged(NetworkLibrary* crosnet) OVERRIDE {
    RefreshNetworkObserver(crosnet);
    RefreshNetworkDeviceObserver(crosnet);
    data_promo_notification_->ShowOptionalMobileDataPromoNotification(
        crosnet, tray_, this);

    NotifyRefreshNetwork();
  }

  // Overridden from NetworkLibrary::NetworkObserver.
  virtual void OnNetworkChanged(NetworkLibrary* crosnet,
      const Network* network) OVERRIDE {
    NotifyRefreshNetwork();
  }

  // Overridden from NetworkLibrary::CellularDataPlanObserver.
  virtual void OnCellularDataPlanChanged(NetworkLibrary* crosnet) OVERRIDE {
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
        ash::UpdateObserver* observer = tray_->update_observer();
        if (observer)
          observer->OnUpdateRecommended(severity);
        break;
      }
      case chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED: {
        // This notification is also sent on login screen when user avatar
        // is loaded from file.
        if (GetUserLoginStatus() != ash::user::LOGGED_IN_NONE) {
          ash::UserObserver* observer = tray_->user_observer();
          if (observer)
            observer->OnUserUpdate();
        }
        break;
      }
      case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED: {
        // GData system service exists by the time if enabled.
        ObserveGDataUpdates();
        break;
      }
      case chrome::NOTIFICATION_PREF_CHANGED: {
        std::string pref = *content::Details<std::string>(details).ptr();
        PrefService* service = content::Source<PrefService>(source).ptr();
        if (pref == prefs::kUse24HourClock) {
          UpdateClockType(service);
        } else if (pref == prefs::kLanguageXkbRemapSearchKeyTo) {
          search_key_mapped_to_ =
              service->GetInteger(prefs::kLanguageXkbRemapSearchKeyTo);
        } else if (pref == prefs::kSpokenFeedbackEnabled) {
          ash::AccessibilityObserver* observer =
              tray_->accessibility_observer();
          if (observer) {
            observer->OnAccessibilityModeChanged(
                service->GetBoolean(prefs::kSpokenFeedbackEnabled));
          }
        } else {
          NOTREACHED();
        }
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
        ash::Shell::GetInstance()->status_area_widget()->
            UpdateAfterLoginStatusChange(GetUserLoginStatus());
        SetProfile(ProfileManager::GetDefaultProfile());
        break;
      }
      default:
        NOTREACHED();
    }
  }

  // Overridden from InputMethodManager::Observer.
  virtual void InputMethodChanged(
      input_method::InputMethodManager* manager, bool show_message) OVERRIDE {
    NotifyRefreshIME(show_message);
  }

  virtual void InputMethodPropertyChanged(
      input_method::InputMethodManager* manager) OVERRIDE {
    NotifyRefreshIME(false);
  }

  // gdata::OperationRegistry::Observer overrides.
  virtual void OnProgressUpdate(
      const OperationRegistry::ProgressStatusList& list) {
    std::vector<ash::DriveOperationStatus> ui_list = GetDriveStatusList(list);
    NotifyRefreshDrive(ui_list);

    // If we have something to report right now (i.e. completion status only),
    // we need to delayed re-check the status in few seconds to ensure we
    // raise events that will let us properly clear the uber tray state.
    if (list.size() > 0) {
      bool has_in_progress_items = false;
      for (OperationRegistry::ProgressStatusList::const_iterator it =
               list.begin();
          it != list.end(); ++it) {
        if (it->transfer_state ==
                OperationRegistry::OPERATION_STARTED ||
            it->transfer_state ==
                OperationRegistry::OPERATION_IN_PROGRESS ||
            it->transfer_state ==
                OperationRegistry::OPERATION_SUSPENDED) {
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

    OnProgressUpdate(system_service->drive_service()->operation_registry()->
        GetProgressStatusList());
  }

  DriveSystemService* FindDriveSystemService() {
    Profile* profile = ProfileManager::GetDefaultProfile();
    if (!gdata::util::IsGDataAvailable(profile))
      return NULL;
    return DriveSystemServiceFactory::FindForProfile(profile);
  }

  // Overridden from system::TimezoneSettings::Observer.
  virtual void TimezoneChanged(const icu::TimeZone& timezone) OVERRIDE {
    NotifyRefreshClock();
  }

  // Overridden from BluetoothAdapter::Observer.
  virtual void AdapterPresentChanged(BluetoothAdapter* adapter,
                                     bool present) OVERRIDE {
    NotifyRefreshBluetooth();
  }

  virtual void AdapterPoweredChanged(BluetoothAdapter* adapter,
                                     bool powered) OVERRIDE {
    NotifyRefreshBluetooth();
  }

  virtual void AdapterDiscoveringChanged(BluetoothAdapter* adapter,
                                         bool discovering) OVERRIDE {
    // TODO: Perhaps start/stop throbbing the icon, or some other visual
    // effects?
  }

  virtual void DeviceAdded(BluetoothAdapter* adapter,
                           BluetoothDevice* device) OVERRIDE {
    NotifyRefreshBluetooth();
  }

  virtual void DeviceChanged(BluetoothAdapter* adapter,
                             BluetoothDevice* device) OVERRIDE {
    NotifyRefreshBluetooth();
  }

  virtual void DeviceRemoved(BluetoothAdapter* adapter,
                             BluetoothDevice* device) OVERRIDE {
    NotifyRefreshBluetooth();
  }

  // Overridden from SystemKeyEventListener::CapsLockObserver.
  virtual void OnCapsLockChange(bool enabled) OVERRIDE {
    bool search_mapped_to_caps_lock = false;
    if (!base::chromeos::IsRunningOnChromeOS() ||
        search_key_mapped_to_ == input_method::kCapsLockKey)
      search_mapped_to_caps_lock = true;

    ash::CapsLockObserver* observer = tray_->caps_lock_observer();
    if (observer)
      observer->OnCapsLockChanged(enabled, search_mapped_to_caps_lock);
  }

  // Overridden from ash::NetworkTrayDelegate
  virtual void NotificationLinkClicked(size_t index) OVERRIDE {
    // If we have deal info URL defined that means that there're
    // 2 links in bubble. Let the user close it manually then thus giving
    // ability to navigate to second link.
    // mobile_data_bubble_ will be set to NULL in BubbleClosing callback.
    std::string deal_info_url = data_promo_notification_->deal_info_url();
    std::string deal_topup_url = data_promo_notification_->deal_topup_url();
    if (deal_info_url.empty())
      data_promo_notification_->CloseNotification();

    std::string deal_url_to_open;
    if (index == 0) {
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
    } else if (index == 1) {
      deal_url_to_open = deal_info_url;
    }

    if (!deal_url_to_open.empty()) {
      Browser* browser = GetAppropriateBrowser();
      if (!browser)
        return;
      chrome::ShowSingletonTab(browser, GURL(deal_url_to_open));
    }
  }

  ash::SystemTray* tray_;
  scoped_ptr<base::WeakPtrFactory<SystemTrayDelegate> > ui_weak_ptr_factory_;
  scoped_ptr<NetworkMenuIcon> network_icon_;
  scoped_ptr<NetworkMenuIcon> network_icon_dark_;
  scoped_ptr<NetworkMenu> network_menu_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<PrefChangeRegistrar> pref_registrar_;
  std::string cellular_device_path_;
  std::string active_network_path_;
  PowerSupplyStatus power_supply_status_;
  base::HourClockType clock_type_;
  int search_key_mapped_to_;
  bool screen_locked_;
  ConnectionState connected_network_state_;
  std::string connected_network_path_;

  scoped_refptr<BluetoothAdapter> bluetooth_adapter_;

  BooleanPrefMember accessibility_enabled_;

  scoped_ptr<DataPromoNotification> data_promo_notification_;

  scoped_ptr<ash::VolumeControlDelegate> volume_control_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegate);
};

}  // namespace

ash::SystemTrayDelegate* CreateSystemTrayDelegate(ash::SystemTray* tray) {
  return new chromeos::SystemTrayDelegate(tray);
}

}  // namespace chromeos
