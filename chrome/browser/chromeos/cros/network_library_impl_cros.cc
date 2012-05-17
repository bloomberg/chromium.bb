// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_library_impl_cros.h"

#include <dbus/dbus-glib.h>
#include "base/command_line.h"
#include "base/json/json_writer.h"  // for debug output only.
#include "base/metrics/histogram.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/native_network_constants.h"
#include "chrome/browser/chromeos/cros/native_network_parser.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// List of cellular operators names that should have data roaming always enabled
// to be able to connect to any network.
const char* kAlwaysInRoamingOperators[] = {
  "CUBIC"
};

// List of interfaces that have portal check enabled by default.
const char kDefaultCheckPortalList[] = "ethernet,wifi,cellular";

}  // namespace

////////////////////////////////////////////////////////////////////////////

NetworkLibraryImplCros::NetworkLibraryImplCros()
    : NetworkLibraryImplBase(),
      weak_ptr_factory_(this) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableLibcros)) {
    LOG(INFO) << "Using Libcros network fucntions.";
    SetLibcrosNetworkFunctionsEnabled(true);
  }
}

NetworkLibraryImplCros::~NetworkLibraryImplCros() {
  STLDeleteValues(&monitored_networks_);
  STLDeleteValues(&monitored_devices_);
}

void NetworkLibraryImplCros::Init() {
  CHECK(CrosLibrary::Get()->libcros_loaded())
      << "libcros must be loaded before NetworkLibraryImplCros::Init()";
  // First, get the currently available networks. This data is cached
  // on the connman side, so the call should be quick.
  VLOG(1) << "Requesting initial network manager info from libcros.";
  CrosRequestNetworkManagerProperties(
      base::Bind(&NetworkLibraryImplCros::NetworkManagerUpdate,
                 weak_ptr_factory_.GetWeakPtr()));
  network_manager_watcher_.reset(CrosMonitorNetworkManagerProperties(
      base::Bind(&NetworkLibraryImplCros::NetworkManagerStatusChangedHandler,
                 weak_ptr_factory_.GetWeakPtr())));
  data_plan_watcher_.reset(
      CrosMonitorCellularDataPlan(
          base::Bind(&NetworkLibraryImplCros::UpdateCellularDataPlan,
                     weak_ptr_factory_.GetWeakPtr())));
  // Always have at least one device obsever so that device updates are
  // always received.
  network_device_observer_.reset(new NetworkLibraryDeviceObserver());
}

bool NetworkLibraryImplCros::IsCros() const {
  return true;
}

//////////////////////////////////////////////////////////////////////////////
// NetworkLibraryImplBase implementation.

void NetworkLibraryImplCros::MonitorNetworkStart(
    const std::string& service_path) {
  if (monitored_networks_.find(service_path) == monitored_networks_.end()) {
    CrosNetworkWatcher* watcher = CrosMonitorNetworkServiceProperties(
        base::Bind(&NetworkLibraryImplCros::UpdateNetworkStatus,
                   weak_ptr_factory_.GetWeakPtr()),
        service_path);
    monitored_networks_[service_path] = watcher;
  }
}

void NetworkLibraryImplCros::MonitorNetworkStop(
    const std::string& service_path) {
  NetworkWatcherMap::iterator iter = monitored_networks_.find(service_path);
  if (iter != monitored_networks_.end()) {
    delete iter->second;
    monitored_networks_.erase(iter);
  }
}

void NetworkLibraryImplCros::MonitorNetworkDeviceStart(
    const std::string& device_path) {
  if (monitored_devices_.find(device_path) == monitored_devices_.end()) {
    CrosNetworkWatcher* watcher = CrosMonitorNetworkDeviceProperties(
        base::Bind(&NetworkLibraryImplCros::UpdateNetworkDeviceStatus,
                   weak_ptr_factory_.GetWeakPtr()),
        device_path);
    monitored_devices_[device_path] = watcher;
  }
}

void NetworkLibraryImplCros::MonitorNetworkDeviceStop(
    const std::string& device_path) {
  NetworkWatcherMap::iterator iter = monitored_devices_.find(device_path);
  if (iter != monitored_devices_.end()) {
    delete iter->second;
    monitored_devices_.erase(iter);
  }
}

void NetworkLibraryImplCros::UpdateNetworkStatus(
    const std::string& path, const std::string& key, const Value& value) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Network* network = FindNetworkByPath(path);
  if (network) {
    VLOG(2) << "UpdateNetworkStatus: " << network->name() << "." << key;
    bool prev_connected = network->connected();
    if (!network->UpdateStatus(key, value, NULL)) {
      LOG(WARNING) << "UpdateNetworkStatus: Error updating: "
                   << path << "." << key;
    }
    // If we just connected, this may have been added to remembered list.
    if (!prev_connected && network->connected())
      RequestRememberedNetworksUpdate();
    NotifyNetworkChanged(network);
    // Anything observing the manager needs to know about any service change.
    NotifyNetworkManagerChanged(false);  // Not forced.
  }
}

void NetworkLibraryImplCros::UpdateNetworkDeviceStatus(
    const std::string& path, const std::string& key, const Value& value) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  NetworkDevice* device = FindNetworkDeviceByPath(path);
  if (device) {
    VLOG(2) << "UpdateNetworkDeviceStatus: " << device->name() << "." << key;
    PropertyIndex index = PROPERTY_INDEX_UNKNOWN;
    if (device->UpdateStatus(key, value, &index)) {
      if (device->type() == TYPE_CELLULAR) {
        if (!UpdateCellularDeviceStatus(device, index))
          return;  // Update aborted, skip notify.
      }
    } else {
      VLOG(1) << "UpdateNetworkDeviceStatus: Failed to update: "
              << path << "." << key;
    }
    // Notify only observers on device property change.
    NotifyNetworkDeviceChanged(device, index);
    // If a device's power state changes, new properties may become defined.
    if (index == PROPERTY_INDEX_POWERED) {
      CrosRequestNetworkDeviceProperties(
          path,
          base::Bind(&NetworkLibraryImplCros::NetworkDeviceUpdate,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

bool NetworkLibraryImplCros::UpdateCellularDeviceStatus(
    NetworkDevice* device, PropertyIndex index) {
  if (index == PROPERTY_INDEX_CELLULAR_ALLOW_ROAMING) {
    if (!device->data_roaming_allowed() && IsCellularAlwaysInRoaming()) {
      SetCellularDataRoamingAllowed(true);
    } else {
      bool settings_value;
      if ((CrosSettings::Get()->GetBoolean(
              kSignedDataRoamingEnabled, &settings_value)) &&
          (device->data_roaming_allowed() != settings_value)) {
        // Switch back to signed settings value.
        SetCellularDataRoamingAllowed(settings_value);
        return false;
      }
    }
  } else if (index == PROPERTY_INDEX_SIM_LOCK) {
    // We only ever request a sim unlock when we wish to enable the device.
    if (!device->is_sim_locked() && !cellular_enabled())
      EnableCellularNetworkDevice(true);
  }
  return true;
}

/////////////////////////////////////////////////////////////////////////////
// NetworkLibraryImplBase connect implementation.

void NetworkLibraryImplCros::CallConfigureService(const std::string& identifier,
                                                  const DictionaryValue* info) {
  CrosConfigureService(*info);
}

void NetworkLibraryImplCros::NetworkConnectCallback(
    const std::string& service_path,
    NetworkMethodErrorType error,
    const std::string& error_message) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkConnectStatus status;
  if (error == NETWORK_METHOD_ERROR_NONE) {
    status = CONNECT_SUCCESS;
  } else {
    LOG(WARNING) << "Error from ServiceConnect callback for: "
                 << service_path
                 << " Error: " << error << " Message: " << error_message;
    if (error_message == flimflam::kErrorPassphraseRequiredMsg) {
      status = CONNECT_BAD_PASSPHRASE;
    } else {
      status = CONNECT_FAILED;
    }
  }
  Network* network = FindNetworkByPath(service_path);
  if (!network) {
    LOG(ERROR) << "No network for path: " << service_path;
    return;
  }
  NetworkConnectCompleted(network, status);
}

void NetworkLibraryImplCros::CallConnectToNetwork(Network* network) {
  DCHECK(network);
  CrosRequestNetworkServiceConnect(
      network->service_path(),
      base::Bind(&NetworkLibraryImplCros::NetworkConnectCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkLibraryImplCros::WifiServiceUpdateAndConnect(
    const std::string& service_path,
    const base::DictionaryValue* properties) {
  if (properties) {
    Network* network = ParseNetwork(service_path, *properties);
    DCHECK_EQ(network->type(), TYPE_WIFI);
    ConnectToWifiNetworkUsingConnectData(static_cast<WifiNetwork*>(network));
  }
}

void NetworkLibraryImplCros::CallRequestWifiNetworkAndConnect(
    const std::string& ssid, ConnectionSecurity security) {
  // Asynchronously request service properties and call
  // WifiServiceUpdateAndConnect.
  CrosRequestHiddenWifiNetworkProperties(
      ssid,
      SecurityToString(security),
      base::Bind(&NetworkLibraryImplCros::WifiServiceUpdateAndConnect,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkLibraryImplCros::VPNServiceUpdateAndConnect(
    const std::string& service_path,
    const base::DictionaryValue* properties) {
  if (properties) {
    VLOG(1) << "Connecting to new VPN Service: " << service_path;
    Network* network = ParseNetwork(service_path, *properties);
    DCHECK_EQ(network->type(), TYPE_VPN);
    ConnectToVirtualNetworkUsingConnectData(
        static_cast<VirtualNetwork*>(network));
  } else {
    LOG(WARNING) << "Unable to create VPN Service: " << service_path;
  }
}

void NetworkLibraryImplCros::CallRequestVirtualNetworkAndConnect(
    const std::string& service_name,
    const std::string& server_hostname,
    ProviderType provider_type) {
  CrosRequestVirtualNetworkProperties(
      service_name,
      server_hostname,
      ProviderTypeToString(provider_type),
      base::Bind(&NetworkLibraryImplCros::VPNServiceUpdateAndConnect,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkLibraryImplCros::CallDeleteRememberedNetwork(
    const std::string& profile_path,
    const std::string& service_path) {
  CrosDeleteServiceFromProfile(profile_path, service_path);
}

//////////////////////////////////////////////////////////////////////////////
// NetworkLibrary implementation.

void NetworkLibraryImplCros::SetCheckPortalList(
    const std::string& check_portal_list) {
  base::StringValue value(check_portal_list);
  CrosSetNetworkManagerProperty(flimflam::kCheckPortalListProperty, value);
}

void NetworkLibraryImplCros::SetDefaultCheckPortalList() {
  SetCheckPortalList(kDefaultCheckPortalList);
}

void NetworkLibraryImplCros::ChangePin(const std::string& old_pin,
                                       const std::string& new_pin) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling ChangePin method w/o cellular device.";
    return;
  }
  sim_operation_ = SIM_OPERATION_CHANGE_PIN;
  CrosRequestChangePin(
      cellular->device_path(), old_pin, new_pin,
      base::Bind(&NetworkLibraryImplCros::PinOperationCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkLibraryImplCros::ChangeRequirePin(bool require_pin,
                                              const std::string& pin) {
  VLOG(1) << "ChangeRequirePin require_pin: " << require_pin
          << " pin: " << pin;
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling ChangeRequirePin method w/o cellular device.";
    return;
  }
  sim_operation_ = SIM_OPERATION_CHANGE_REQUIRE_PIN;
  CrosRequestRequirePin(
      cellular->device_path(), pin, require_pin,
      base::Bind(&NetworkLibraryImplCros::PinOperationCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkLibraryImplCros::EnterPin(const std::string& pin) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling EnterPin method w/o cellular device.";
    return;
  }
  sim_operation_ = SIM_OPERATION_ENTER_PIN;
  CrosRequestEnterPin(cellular->device_path(), pin,
                      base::Bind(&NetworkLibraryImplCros::PinOperationCallback,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkLibraryImplCros::UnblockPin(const std::string& puk,
                                        const std::string& new_pin) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling UnblockPin method w/o cellular device.";
    return;
  }
  sim_operation_ = SIM_OPERATION_UNBLOCK_PIN;
  CrosRequestUnblockPin(
      cellular->device_path(), puk, new_pin,
      base::Bind(&NetworkLibraryImplCros::PinOperationCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkLibraryImplCros::PinOperationCallback(
    const std::string& path,
    NetworkMethodErrorType error,
    const std::string& error_message) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  PinOperationError pin_error;
  VLOG(1) << "PinOperationCallback, error: " << error
          << " error_msg: " << error_message;
  if (error == chromeos::NETWORK_METHOD_ERROR_NONE) {
    pin_error = PIN_ERROR_NONE;
    VLOG(1) << "Pin operation completed successfuly";
  } else {
    if (error_message == flimflam::kErrorIncorrectPinMsg ||
        error_message == flimflam::kErrorPinRequiredMsg) {
      pin_error = PIN_ERROR_INCORRECT_CODE;
    } else if (error_message == flimflam::kErrorPinBlockedMsg) {
      pin_error = PIN_ERROR_BLOCKED;
    } else {
      pin_error = PIN_ERROR_UNKNOWN;
      NOTREACHED() << "Unknown PIN error: " << error_message;
    }
  }
  NotifyPinOperationCompleted(pin_error);
}

void NetworkLibraryImplCros::RequestCellularScan() {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling RequestCellularScan method w/o cellular device.";
    return;
  }
  CrosProposeScan(cellular->device_path());
}

void NetworkLibraryImplCros::RequestCellularRegister(
    const std::string& network_id) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling CellularRegister method w/o cellular device.";
    return;
  }
  CrosRequestCellularRegister(
      cellular->device_path(), network_id,
      base::Bind(&NetworkLibraryImplCros::CellularRegisterCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetworkLibraryImplCros::CellularRegisterCallback(
    const std::string& path,
    NetworkMethodErrorType error,
    const std::string& error_message) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  // TODO(dpolukhin): Notify observers about network registration status
  // but not UI doesn't assume such notification so just ignore result.
}

void NetworkLibraryImplCros::SetCellularDataRoamingAllowed(bool new_value) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling SetCellularDataRoamingAllowed method "
        "w/o cellular device.";
    return;
  }
  base::FundamentalValue value(new_value);
  CrosSetNetworkDeviceProperty(cellular->device_path(),
                               flimflam::kCellularAllowRoamingProperty,
                               value);
}

bool NetworkLibraryImplCros::IsCellularAlwaysInRoaming() {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling IsCellularAlwaysInRoaming method "
        "w/o cellular device.";
    return false;
  }
  const std::string& home_provider_name = cellular->home_provider_name();
  for (size_t i = 0; i < arraysize(kAlwaysInRoamingOperators); i++) {
    if (home_provider_name == kAlwaysInRoamingOperators[i])
      return true;
  }
  return false;
}

void NetworkLibraryImplCros::RequestNetworkScan() {
  if (wifi_enabled()) {
    wifi_scanning_ = true;  // Cleared when updates are received.
    CrosRequestNetworkScan(flimflam::kTypeWifi);
  }

  if (wimax_enabled())
    CrosRequestNetworkScan(flimflam::kTypeWimax);

  if (cellular_network())
    cellular_network()->RefreshDataPlansIfNeeded();
  // Make sure all Manager info is up to date. This will also update
  // remembered networks and visible services.
  CrosRequestNetworkManagerProperties(
      base::Bind(&NetworkLibraryImplCros::NetworkManagerUpdate,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool NetworkLibraryImplCros::GetWifiAccessPoints(
    WifiAccessPointVector* result) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return CrosGetWifiAccessPoints(result);
}

void NetworkLibraryImplCros::DisconnectFromNetwork(const Network* network) {
  DCHECK(network);
  // Asynchronous disconnect request. Network state will be updated through
  // the network manager once disconnect completes.
  CrosRequestNetworkServiceDisconnect(network->service_path());
}

void NetworkLibraryImplCros::CallEnableNetworkDeviceType(
    ConnectionType device, bool enable) {
  busy_devices_ |= 1 << device;
  CrosRequestNetworkDeviceEnable(ConnectionTypeToString(device), enable);
}

void NetworkLibraryImplCros::CallRemoveNetwork(const Network* network) {
  const std::string& service_path = network->service_path();
  if (network->connected())
    CrosRequestNetworkServiceDisconnect(service_path);
  CrosRequestRemoveNetworkService(service_path);
}

void NetworkLibraryImplCros::EnableOfflineMode(bool enable) {
  // If network device is already enabled/disabled, then don't do anything.
  if (CrosSetOfflineMode(enable))
    offline_mode_ = enable;
}

NetworkIPConfigVector NetworkLibraryImplCros::GetIPConfigs(
    const std::string& device_path,
    std::string* hardware_address,
    HardwareAddressFormat format) {
  NetworkIPConfigVector ipconfig_vector;
  CrosListIPConfigs(device_path, &ipconfig_vector, NULL, hardware_address);

  for (size_t i = 0; i < hardware_address->size(); ++i)
    (*hardware_address)[i] = toupper((*hardware_address)[i]);
  if (format == FORMAT_COLON_SEPARATED_HEX) {
    if (hardware_address->size() % 2 == 0) {
      std::string output;
      for (size_t i = 0; i < hardware_address->size(); ++i) {
        if ((i != 0) && (i % 2 == 0))
          output.push_back(':');
        output.push_back((*hardware_address)[i]);
      }
      *hardware_address = output;
    }
  } else {
    DCHECK_EQ(format, FORMAT_RAW_HEX);
  }
  return ipconfig_vector;
}

void NetworkLibraryImplCros::SetIPConfig(const NetworkIPConfig& ipconfig) {
  if (ipconfig.device_path.empty())
    return;

  NetworkIPConfig* ipconfig_dhcp = NULL;
  std::string ipconfig_dhcp_path;
  NetworkIPConfig* ipconfig_static = NULL;
  std::string ipconfig_static_path;

  NetworkIPConfigVector ipconfigs;
  std::vector<std::string> ipconfig_paths;
  CrosListIPConfigs(ipconfig.device_path, &ipconfigs, &ipconfig_paths, NULL);
  for (size_t i = 0; i < ipconfigs.size(); ++i) {
    if (ipconfigs[i].type == chromeos::IPCONFIG_TYPE_DHCP) {
      ipconfig_dhcp = &ipconfigs[i];
      ipconfig_dhcp_path = ipconfig_paths[i];
    } else if (ipconfigs[i].type == chromeos::IPCONFIG_TYPE_IPV4) {
      ipconfig_static = &ipconfigs[i];
      ipconfig_static_path = ipconfig_paths[i];
    }
  }

  NetworkIPConfigVector ipconfigs2;
  std::vector<std::string> ipconfig_paths2;
  if (ipconfig.type == chromeos::IPCONFIG_TYPE_DHCP) {
    // If switching from static to dhcp, create new dhcp ip config.
    if (!ipconfig_dhcp)
      CrosAddIPConfig(ipconfig.device_path, chromeos::IPCONFIG_TYPE_DHCP);
    // User wants DHCP now. So delete the static ip config.
    if (ipconfig_static)
      CrosRemoveIPConfig(ipconfig_static_path);
  } else if (ipconfig.type == chromeos::IPCONFIG_TYPE_IPV4) {
    // If switching from dhcp to static, create new static ip config.
    if (!ipconfig_static) {
      CrosAddIPConfig(ipconfig.device_path, chromeos::IPCONFIG_TYPE_IPV4);
      // Now find the newly created IP config.
      if (CrosListIPConfigs(ipconfig.device_path, &ipconfigs2,
                            &ipconfig_paths2, NULL)) {
        for (size_t i = 0; i < ipconfigs2.size(); ++i) {
          if (ipconfigs2[i].type == chromeos::IPCONFIG_TYPE_IPV4) {
            ipconfig_static = &ipconfigs2[i];
            ipconfig_static_path = ipconfig_paths2[i];
          }
        }
      }
    }
    if (ipconfig_static) {
      // Save any changed details.
      if (ipconfig.address != ipconfig_static->address) {
        base::StringValue value(ipconfig.address);
        CrosSetNetworkIPConfigProperty(ipconfig_static->device_path,
                                       flimflam::kAddressProperty, value);
      }
      if (ipconfig.netmask != ipconfig_static->netmask) {
        int prefixlen = ipconfig.GetPrefixLength();
        if (prefixlen == -1) {
          VLOG(1) << "IP config prefixlen is invalid for netmask "
                  << ipconfig.netmask;
        } else {
          base::FundamentalValue value(prefixlen);
          CrosSetNetworkIPConfigProperty(ipconfig_static->device_path,
                                         flimflam::kPrefixlenProperty, value);
        }
      }
      if (ipconfig.gateway != ipconfig_static->gateway) {
        base::StringValue value(ipconfig.gateway);
        CrosSetNetworkIPConfigProperty(ipconfig_static->device_path,
                                       flimflam::kGatewayProperty, value);
      }
      if (ipconfig.name_servers != ipconfig_static->name_servers) {
        base::StringValue value(ipconfig.name_servers);
        CrosSetNetworkIPConfigProperty(ipconfig_static->device_path,
                                       flimflam::kNameServersProperty, value);
      }
      // Remove dhcp ip config if there is one.
      if (ipconfig_dhcp)
        CrosRemoveIPConfig(ipconfig_dhcp_path);
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// Network Manager functions.

void NetworkLibraryImplCros::NetworkManagerStatusChangedHandler(
    const std::string& path,
    const std::string& key,
    const Value& value) {
  NetworkManagerStatusChanged(key, &value);
}

// This processes all Manager update messages.
void NetworkLibraryImplCros::NetworkManagerStatusChanged(
    const std::string& key, const Value* value) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::TimeTicks start = base::TimeTicks::Now();
  VLOG(1) << "NetworkManagerStatusChanged: KEY=" << key;
  int index = NativeNetworkParser::property_mapper()->Get(key);
  switch (index) {
    case PROPERTY_INDEX_STATE:
      // Currently we ignore the network manager state.
      break;
    case PROPERTY_INDEX_AVAILABLE_TECHNOLOGIES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateAvailableTechnologies(vlist);
      break;
    }
    case PROPERTY_INDEX_ENABLED_TECHNOLOGIES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateEnabledTechnologies(vlist);
      break;
    }
    case PROPERTY_INDEX_CONNECTED_TECHNOLOGIES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateConnectedTechnologies(vlist);
      break;
    }
    case PROPERTY_INDEX_DEFAULT_TECHNOLOGY:
      // Currently we ignore DefaultTechnology.
      break;
    case PROPERTY_INDEX_OFFLINE_MODE: {
      DCHECK_EQ(value->GetType(), Value::TYPE_BOOLEAN);
      value->GetAsBoolean(&offline_mode_);
      NotifyNetworkManagerChanged(false);  // Not forced.
      break;
    }
    case PROPERTY_INDEX_ACTIVE_PROFILE: {
      std::string prev = active_profile_path_;
      DCHECK_EQ(value->GetType(), Value::TYPE_STRING);
      value->GetAsString(&active_profile_path_);
      VLOG(1) << "Active Profile: " << active_profile_path_;
      if (active_profile_path_ != prev &&
          active_profile_path_ != kSharedProfilePath)
        SwitchToPreferredNetwork();
      break;
    }
    case PROPERTY_INDEX_PROFILES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateRememberedNetworks(vlist);
      RequestRememberedNetworksUpdate();
      break;
    }
    case PROPERTY_INDEX_SERVICES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateNetworkServiceList(vlist);
      break;
    }
    case PROPERTY_INDEX_SERVICE_WATCH_LIST: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateWatchedNetworkServiceList(vlist);
      break;
    }
    case PROPERTY_INDEX_DEVICE:
    case PROPERTY_INDEX_DEVICES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateNetworkDeviceList(vlist);
      break;
    }
    case PROPERTY_INDEX_CHECK_PORTAL_LIST: {
      DCHECK_EQ(value->GetType(), Value::TYPE_STRING);
      value->GetAsString(&check_portal_list_);
      break;
    }
    case PROPERTY_INDEX_PORTAL_URL:
    case PROPERTY_INDEX_ARP_GATEWAY:
      // Currently we ignore PortalURL and ArpGateway.
      break;
    default:
      LOG(WARNING) << "Manager: Unhandled key: " << key;
      break;
  }
  base::TimeDelta delta = base::TimeTicks::Now() - start;
  VLOG(2) << "NetworkManagerStatusChanged: time: "
          << delta.InMilliseconds() << " ms.";
  HISTOGRAM_TIMES("CROS_NETWORK_UPDATE", delta);
}

void NetworkLibraryImplCros::NetworkManagerUpdate(
    const std::string& manager_path,
    const base::DictionaryValue* properties) {
  if (!properties) {
    LOG(ERROR) << "Error retrieving manager properties: " << manager_path;
    return;
  }
  VLOG(1) << "Received NetworkManagerUpdate.";

  for (DictionaryValue::key_iterator iter = properties->begin_keys();
       iter != properties->end_keys(); ++iter) {
    const std::string& key = *iter;
    Value* value;
    bool res = properties->GetWithoutPathExpansion(key, &value);
    CHECK(res);
    NetworkManagerStatusChanged(key, value);
  }
  // If there is no Profiles entry, request remembered networks here.
  if (!properties->HasKey(flimflam::kProfilesProperty))
    RequestRememberedNetworksUpdate();
}

////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplCros::UpdateTechnologies(
    const ListValue* technologies, int* bitfieldp) {
  DCHECK(bitfieldp);
  if (!technologies)
    return;
  int bitfield = 0;
  for (ListValue::const_iterator iter = technologies->begin();
       iter != technologies->end(); ++iter) {
    std::string technology;
    (*iter)->GetAsString(&technology);
    if (!technology.empty()) {
      ConnectionType type =
          NativeNetworkParser::ParseConnectionType(technology);
      bitfield |= 1 << type;
    }
  }
  *bitfieldp = bitfield;
  NotifyNetworkManagerChanged(false);  // Not forced.
}

void NetworkLibraryImplCros::UpdateAvailableTechnologies(
    const ListValue* technologies) {
  UpdateTechnologies(technologies, &available_devices_);
}

void NetworkLibraryImplCros::UpdateEnabledTechnologies(
    const ListValue* technologies) {
  int old_enabled_devices = enabled_devices_;
  UpdateTechnologies(technologies, &enabled_devices_);
  busy_devices_ &= ~(old_enabled_devices ^ enabled_devices_);
  if (!ethernet_enabled())
    ethernet_ = NULL;
  if (!wifi_enabled()) {
    active_wifi_ = NULL;
    wifi_networks_.clear();
  }
  if (!cellular_enabled()) {
    active_cellular_ = NULL;
    cellular_networks_.clear();
  }
}

void NetworkLibraryImplCros::UpdateConnectedTechnologies(
    const ListValue* technologies) {
  UpdateTechnologies(technologies, &connected_devices_);
}

////////////////////////////////////////////////////////////////////////////

// Update all network lists, and request associated service updates.
void NetworkLibraryImplCros::UpdateNetworkServiceList(
    const ListValue* services) {
  // TODO(stevenjb): Wait for wifi_scanning_ to be false.
  // Copy the list of existing networks to "old" and clear the map and lists.
  NetworkMap old_network_map = network_map_;
  ClearNetworks();
  // Clear the list of update requests.
  int network_priority_order = 0;
  network_update_requests_.clear();
  // wifi_scanning_ will remain false unless we request a network update.
  wifi_scanning_ = false;
  // |services| represents a complete list of visible networks.
  for (ListValue::const_iterator iter = services->begin();
       iter != services->end(); ++iter) {
    std::string service_path;
    (*iter)->GetAsString(&service_path);
    if (!service_path.empty()) {
      // If we find the network in "old", add it immediately to the map
      // and lists. Otherwise it will get added when NetworkServiceUpdate
      // calls ParseNetwork.
      NetworkMap::iterator found = old_network_map.find(service_path);
      if (found != old_network_map.end()) {
        AddNetwork(found->second);
        old_network_map.erase(found);
      }
      // Always request network updates.
      // TODO(stevenjb): Investigate why we are missing updates then
      // rely on watched network updates and only request updates here for
      // new networks.
      // Use update_request map to store network priority.
      network_update_requests_[service_path] = network_priority_order++;
      wifi_scanning_ = true;
      CrosRequestNetworkServiceProperties(
          service_path,
          base::Bind(&NetworkLibraryImplCros::NetworkServiceUpdate,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }
  // Iterate through list of remaining networks that are no longer in the
  // list and delete them or update their status and re-add them to the list.
  for (NetworkMap::iterator iter = old_network_map.begin();
       iter != old_network_map.end(); ++iter) {
    Network* network = iter->second;
    VLOG(2) << "Delete Network: " << network->name()
            << " State = " << network->GetStateString()
            << " connecting = " << network->connecting()
            << " connection_started = " << network->connection_started();
    if (network->failed() && network->notify_failure()) {
      // We have not notified observers of a connection failure yet.
      AddNetwork(network);
    } else if (network->connecting() && network->connection_started()) {
      // Network was in connecting state; set state to failed.
      VLOG(2) << "Removed network was connecting: " << network->name();
      network->SetState(STATE_FAILURE);
      AddNetwork(network);
    } else {
      VLOG(2) << "Deleting removed network: " << network->name()
              << " State = " << network->GetStateString();
      DeleteNetwork(network);
    }
  }
  // If the last network has disappeared, nothing else will
  // have notified observers, so do it now.
  if (services->empty())
    NotifyNetworkManagerChanged(true);  // Forced update
}

// Request updates for watched networks. Does not affect network lists.
// Existing networks will be updated. There should not be any new networks
// in this list, but if there are they will be added appropriately.
void NetworkLibraryImplCros::UpdateWatchedNetworkServiceList(
    const ListValue* services) {
  for (ListValue::const_iterator iter = services->begin();
       iter != services->end(); ++iter) {
    std::string service_path;
    (*iter)->GetAsString(&service_path);
    if (!service_path.empty()) {
      VLOG(1) << "Watched Service: " << service_path;
      CrosRequestNetworkServiceProperties(
          service_path,
          base::Bind(&NetworkLibraryImplCros::NetworkServiceUpdate,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void NetworkLibraryImplCros::NetworkServiceUpdate(
    const std::string& service_path,
    const base::DictionaryValue* properties) {
  if (!properties)
    return;  // Network no longer in visible list, ignore.
  ParseNetwork(service_path, *properties);
}

// Called from NetworkServiceUpdate and WifiServiceUpdateAndConnect.
Network* NetworkLibraryImplCros::ParseNetwork(
    const std::string& service_path, const DictionaryValue& info) {
  Network* network = FindNetworkByPath(service_path);
  if (!network) {
    NativeNetworkParser parser;
    network = parser.CreateNetworkFromInfo(service_path, info);
    AddNetwork(network);
  } else {
    // Erase entry from network_unique_id_map_ in case unique id changes.
    if (!network->unique_id().empty())
      network_unique_id_map_.erase(network->unique_id());
    if (network->network_parser()) {
      ConnectionState old_state = network->state();
      network->network_parser()->UpdateNetworkFromInfo(info, network);
      if (old_state != network->state()) {
        VLOG(1) << "ParseNetwork: " << network->name()
                << " State: " << old_state << " -> " << network->state();
      }
    }
  }

  if (!network->unique_id().empty())
    network_unique_id_map_[network->unique_id()] = network;

  SetProfileTypeFromPath(network);

  UpdateActiveNetwork(network);

  // Copy remembered credentials if required.
  Network* remembered = FindRememberedFromNetwork(network);
  if (remembered)
    network->CopyCredentialsFromRemembered(remembered);

  // Find and erase entry in update_requests, and set network priority.
  PriorityMap::iterator found2 = network_update_requests_.find(service_path);
  if (found2 != network_update_requests_.end()) {
    network->priority_order_ = found2->second;
    network_update_requests_.erase(found2);
    if (network_update_requests_.empty()) {
      // Clear wifi_scanning_ when we have no pending requests.
      wifi_scanning_ = false;
    }
  } else {
    // TODO(stevenjb): Enable warning once UpdateNetworkServiceList is fixed.
    // LOG(WARNING) << "ParseNetwork called with no update request entry: "
    //              << service_path;
  }

  VLOG(2) << "ParseNetwork: " << network->name()
          << " path: " << network->service_path()
          << " profile: " << network->profile_path_;
  NotifyNetworkManagerChanged(false);  // Not forced.
  return network;
}

////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplCros::UpdateRememberedNetworks(
    const ListValue* profiles) {
  VLOG(1) << "UpdateRememberedNetworks";
  // Update the list of profiles.
  profile_list_.clear();
  for (ListValue::const_iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    std::string profile_path;
    (*iter)->GetAsString(&profile_path);
    if (profile_path.empty()) {
      LOG(WARNING) << "Bad or empty profile path.";
      continue;
    }
    NetworkProfileType profile_type;
    if (profile_path == kSharedProfilePath)
      profile_type = PROFILE_SHARED;
    else
      profile_type = PROFILE_USER;
    AddProfile(profile_path, profile_type);
  }
}

void NetworkLibraryImplCros::RequestRememberedNetworksUpdate() {
  VLOG(1) << "RequestRememberedNetworksUpdate";
  // Delete all remembered networks. We delete them because
  // RequestNetworkProfileProperties is asynchronous and may invoke
  // UpdateRememberedServiceList multiple times (once per profile).
  // We can do this safely because we do not save any local state for
  // remembered networks. This list updates infrequently.
  DeleteRememberedNetworks();
  // Request remembered networks from each profile. Later entries will
  // override earlier entries, so default/local entries will override
  // user entries (as desired).
  for (NetworkProfileList::iterator iter = profile_list_.begin();
       iter != profile_list_.end(); ++iter) {
    NetworkProfile& profile = *iter;
    VLOG(1) << " Requesting Profile: " << profile.path;
    CrosRequestNetworkProfileProperties(
        profile.path,
        base::Bind(&NetworkLibraryImplCros::UpdateProfile,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void NetworkLibraryImplCros::UpdateProfile(
    const std::string& profile_path,
    const base::DictionaryValue* properties) {
  if (!properties) {
    LOG(ERROR) << "Error retrieving profile: " << profile_path;
    return;
  }
  VLOG(1) << "UpdateProfile for path: " << profile_path;
  ListValue* profile_entries(NULL);
  properties->GetList(flimflam::kEntriesProperty, &profile_entries);
  if (!profile_entries) {
    LOG(ERROR) << "'Entries' property is missing.";
    return;
  }

  NetworkProfileList::iterator iter1;
  for (iter1 = profile_list_.begin(); iter1 != profile_list_.end(); ++iter1) {
    if (iter1->path == profile_path)
      break;
  }
  if (iter1 == profile_list_.end()) {
    // This can happen if flimflam gets restarted while Chrome is running.
    LOG(WARNING) << "Profile not in list: " << profile_path;
    return;
  }
  NetworkProfile& profile = *iter1;
  // |profile_entries| is a list of remembered networks from |profile_path|.
  profile.services.clear();
  for (ListValue::const_iterator iter2 = profile_entries->begin();
       iter2 != profile_entries->end(); ++iter2) {
    std::string service_path;
    (*iter2)->GetAsString(&service_path);
    if (service_path.empty()) {
      LOG(WARNING) << "Empty service path in profile.";
      continue;
    }
    VLOG(1) << " Remembered service: " << service_path;
    // Add service to profile list.
    profile.services.insert(service_path);
    // Request update for remembered network.
    CrosRequestNetworkProfileEntryProperties(
        profile_path,
        service_path,
        base::Bind(&NetworkLibraryImplCros::RememberedNetworkServiceUpdate,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void NetworkLibraryImplCros::RememberedNetworkServiceUpdate(
    const std::string& service_path,
    const base::DictionaryValue* properties) {
  if (!properties) {
    // Remembered network no longer exists.
    DeleteRememberedNetwork(service_path);
  } else {
    ParseRememberedNetwork(service_path, *properties);
  }
}

// Returns NULL if |service_path| refers to a network that is not a
// remembered type. Called from RememberedNetworkServiceUpdate.
Network* NetworkLibraryImplCros::ParseRememberedNetwork(
    const std::string& service_path, const DictionaryValue& info) {
  Network* remembered;
  NetworkMap::iterator found = remembered_network_map_.find(service_path);
  if (found != remembered_network_map_.end()) {
    remembered = found->second;
    // Erase entry from network_unique_id_map_ in case unique id changes.
    if (!remembered->unique_id().empty())
      remembered_network_unique_id_map_.erase(remembered->unique_id());
    if (remembered->network_parser())
      remembered->network_parser()->UpdateNetworkFromInfo(info, remembered);
  } else {
    NativeNetworkParser parser;
    remembered = parser.CreateNetworkFromInfo(service_path, info);
    if (remembered->type() == TYPE_WIFI || remembered->type() == TYPE_VPN) {
      if (!ValidateAndAddRememberedNetwork(remembered))
        return NULL;
    } else {
      LOG(WARNING) << "Ignoring remembered network: " << service_path
                   << " Type: " << ConnectionTypeToString(remembered->type());
      delete remembered;
      return NULL;
    }
  }

  if (!remembered->unique_id().empty())
    remembered_network_unique_id_map_[remembered->unique_id()] = remembered;

  SetProfileTypeFromPath(remembered);

  VLOG(1) << "ParseRememberedNetwork: " << remembered->name()
          << " path: " << remembered->service_path()
          << " profile: " << remembered->profile_path_;
  NotifyNetworkManagerChanged(false);  // Not forced.

  if (remembered->type() == TYPE_VPN) {
    // VPNs are only stored in profiles. If we don't have a network for it,
    // request one.
    if (!FindNetworkByUniqueId(remembered->unique_id())) {
      VirtualNetwork* vpn = static_cast<VirtualNetwork*>(remembered);
      std::string provider_type = ProviderTypeToString(vpn->provider_type());
      VLOG(1) << "Requesting VPN: " << vpn->name()
              << " Server: " << vpn->server_hostname()
              << " Type: " << provider_type;
      CrosRequestVirtualNetworkProperties(
          vpn->name(),
          vpn->server_hostname(),
          provider_type,
          base::Bind(&NetworkLibraryImplCros::NetworkServiceUpdate,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

  return remembered;
}

////////////////////////////////////////////////////////////////////////////
// NetworkDevice list management functions.

// Update device list, and request associated device updates.
// |devices| represents a complete list of devices.
void NetworkLibraryImplCros::UpdateNetworkDeviceList(const ListValue* devices) {
  NetworkDeviceMap old_device_map = device_map_;
  device_map_.clear();
  VLOG(2) << "Updating Device List.";
  for (ListValue::const_iterator iter = devices->begin();
       iter != devices->end(); ++iter) {
    std::string device_path;
    (*iter)->GetAsString(&device_path);
    if (!device_path.empty()) {
      NetworkDeviceMap::iterator found = old_device_map.find(device_path);
      if (found != old_device_map.end()) {
        VLOG(2) << " Adding existing device: " << device_path;
        CHECK(found->second) << "Attempted to add NULL device pointer";
        device_map_[device_path] = found->second;
        old_device_map.erase(found);
      }
      CrosRequestNetworkDeviceProperties(
          device_path,
          base::Bind(&NetworkLibraryImplCros::NetworkDeviceUpdate,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }
  // Delete any old devices that no longer exist.
  for (NetworkDeviceMap::iterator iter = old_device_map.begin();
       iter != old_device_map.end(); ++iter) {
    DeleteDeviceFromDeviceObserversMap(iter->first);
    // Delete device.
    delete iter->second;
  }
}

void NetworkLibraryImplCros::NetworkDeviceUpdate(
    const std::string& device_path,
    const base::DictionaryValue* properties) {
  if (!properties) {
    // device no longer exists.
    DeleteDevice(device_path);
  } else {
    ParseNetworkDevice(device_path, *properties);
  }
}

void NetworkLibraryImplCros::ParseNetworkDevice(const std::string& device_path,
                                                const DictionaryValue& info) {
  NetworkDeviceMap::iterator found = device_map_.find(device_path);
  NetworkDevice* device;
  if (found != device_map_.end()) {
    device = found->second;
    device->device_parser()->UpdateDeviceFromInfo(info, device);
  } else {
    NativeNetworkDeviceParser parser;
    device = parser.CreateDeviceFromInfo(device_path, info);
    VLOG(2) << " Adding device: " << device_path;
    if (device) {
      device_map_[device_path] = device;
    }
    CHECK(device) << "Attempted to add NULL device for path: " << device_path;
  }
  VLOG(1) << "ParseNetworkDevice:" << device->name();
  if (device && device->type() == TYPE_CELLULAR) {
    if (!device->data_roaming_allowed() && IsCellularAlwaysInRoaming()) {
      SetCellularDataRoamingAllowed(true);
    } else {
      bool settings_value;
      if (CrosSettings::Get()->GetBoolean(
              kSignedDataRoamingEnabled, &settings_value) &&
          device->data_roaming_allowed() != settings_value) {
        // Switch back to signed settings value.
        SetCellularDataRoamingAllowed(settings_value);
      }
    }
  }
  NotifyNetworkManagerChanged(false);  // Not forced.
  AddNetworkDeviceObserver(device_path, network_device_observer_.get());
}

}  // namespace chromeos
