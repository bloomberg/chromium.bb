// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_library_impl_cros.h"

#include <dbus/dbus-glib.h>
#include "base/json/json_writer.h"  // for debug output only.
#include "base/metrics/histogram.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/gvalue_util.h"
#include "chrome/browser/chromeos/cros/native_network_constants.h"
#include "chrome/browser/chromeos/cros/native_network_parser.h"
#include "chrome/browser/chromeos/cros_settings.h"
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

// Safe string constructor since we can't rely on non NULL pointers
// for string values from libcros.
std::string SafeString(const char* s) {
  return s ? std::string(s) : std::string();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////

NetworkLibraryImplCros::NetworkLibraryImplCros()
    : NetworkLibraryImplBase(),
      network_manager_monitor_(NULL),
      data_plan_monitor_(NULL) {
}

NetworkLibraryImplCros::~NetworkLibraryImplCros() {
  if (network_manager_monitor_)
    CrosDisconnectNetworkPropertiesMonitor(network_manager_monitor_);
  if (data_plan_monitor_)
    CrosDisconnectDataPlanUpdateMonitor(data_plan_monitor_);
  for (NetworkPropertiesMonitorMap::iterator iter =
           montitored_networks_.begin();
       iter != montitored_networks_.end(); ++iter) {
    CrosDisconnectNetworkPropertiesMonitor(iter->second);
  }
  for (NetworkPropertiesMonitorMap::iterator iter =
           montitored_devices_.begin();
       iter != montitored_devices_.end(); ++iter) {
    CrosDisconnectNetworkPropertiesMonitor(iter->second);
  }
}

void NetworkLibraryImplCros::Init() {
  CHECK(CrosLibrary::Get()->libcros_loaded())
      << "libcros must be loaded before NetworkLibraryImplCros::Init()";
  // First, get the currently available networks. This data is cached
  // on the connman side, so the call should be quick.
  VLOG(1) << "Requesting initial network manager info from libcros.";
  CrosRequestNetworkManagerProperties(base::Bind(&NetworkManagerUpdate, this));
  network_manager_monitor_ =
      CrosMonitorNetworkManagerProperties(
          &NetworkManagerStatusChangedHandler, this);
  data_plan_monitor_ =
      CrosMonitorCellularDataPlan(&DataPlanUpdateHandler, this);
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
  if (montitored_networks_.find(service_path) == montitored_networks_.end()) {
    chromeos::NetworkPropertiesMonitor monitor =
        CrosMonitorNetworkServiceProperties(
            &NetworkStatusChangedHandler, service_path.c_str(), this);
    montitored_networks_[service_path] = monitor;
  }
}

void NetworkLibraryImplCros::MonitorNetworkStop(
    const std::string& service_path) {
  NetworkPropertiesMonitorMap::iterator iter =
      montitored_networks_.find(service_path);
  if (iter != montitored_networks_.end()) {
    CrosDisconnectNetworkPropertiesMonitor(iter->second);
    montitored_networks_.erase(iter);
  }
}

void NetworkLibraryImplCros::MonitorNetworkDeviceStart(
    const std::string& device_path) {
  if (montitored_devices_.find(device_path) == montitored_devices_.end()) {
    chromeos::NetworkPropertiesMonitor monitor =
        CrosMonitorNetworkDeviceProperties(
            &NetworkDevicePropertyChangedHandler, device_path.c_str(), this);
    montitored_devices_[device_path] = monitor;
  }
}

void NetworkLibraryImplCros::MonitorNetworkDeviceStop(
    const std::string& device_path) {
  NetworkPropertiesMonitorMap::iterator iter =
      montitored_devices_.find(device_path);
  if (iter != montitored_devices_.end()) {
    CrosDisconnectNetworkPropertiesMonitor(iter->second);
    montitored_devices_.erase(iter);
  }
}

// static callback
void NetworkLibraryImplCros::NetworkStatusChangedHandler(
    void* object, const char* path, const char* key, const GValue* gvalue) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (key == NULL || gvalue == NULL || path == NULL || object == NULL)
    return;
  scoped_ptr<Value> value(ConvertGValueToValue(gvalue));
  networklib->UpdateNetworkStatus(std::string(path), std::string(key), *value);
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

// static callback
void NetworkLibraryImplCros::NetworkDevicePropertyChangedHandler(
    void* object, const char* path, const char* key, const GValue* gvalue) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (key == NULL || gvalue == NULL || path == NULL || object == NULL)
    return;
  scoped_ptr<Value> value(ConvertGValueToValue(gvalue));
  networklib->UpdateNetworkDeviceStatus(std::string(path),
                                        std::string(key),
                                        *value);
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
      CrosRequestNetworkDeviceProperties(path.c_str(),
                                         base::Bind(&NetworkDeviceUpdate,
                                                    this));
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

// static callback
void NetworkLibraryImplCros::ConfigureServiceCallback(
    void* object,
    const char* service_path,
    NetworkMethodErrorType error,
    const char* error_message) {
  if (error != NETWORK_METHOD_ERROR_NONE) {
    LOG(WARNING) << "Error from ConfigureService callback for: "
                 << service_path
                 << " Error: " << error << " Message: " << error_message;
  }
}

void NetworkLibraryImplCros::CallConfigureService(const std::string& identifier,
                                                  const DictionaryValue* info) {
  CrosConfigureService(identifier.c_str(), *info,
                       ConfigureServiceCallback, this);
}

// static callback
void NetworkLibraryImplCros::NetworkConnectCallback(
    void* object,
    const char* service_path,
    NetworkMethodErrorType error,
    const char* error_message) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkConnectStatus status;
  if (error == NETWORK_METHOD_ERROR_NONE) {
    status = CONNECT_SUCCESS;
  } else {
    LOG(WARNING) << "Error from ServiceConnect callback for: "
                 << service_path
                 << " Error: " << error << " Message: " << error_message;
    if (error_message &&
        strcmp(error_message, flimflam::kErrorPassphraseRequiredMsg) == 0) {
      status = CONNECT_BAD_PASSPHRASE;
    } else {
      status = CONNECT_FAILED;
    }
  }
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  Network* network = networklib->FindNetworkByPath(std::string(service_path));
  if (!network) {
    LOG(ERROR) << "No network for path: " << service_path;
    return;
  }
  networklib->NetworkConnectCompleted(network, status);
}

void NetworkLibraryImplCros::CallConnectToNetwork(Network* network) {
  DCHECK(network);
  CrosRequestNetworkServiceConnect(network->service_path().c_str(),
                                   NetworkConnectCallback, this);
}

// static callback
void NetworkLibraryImplCros::WifiServiceUpdateAndConnect(
    void* object,
    const char* service_path,
    const base::DictionaryValue* properties) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (service_path && properties) {
    Network* network =
        networklib->ParseNetwork(std::string(service_path), *properties);
    DCHECK_EQ(network->type(), TYPE_WIFI);
    networklib->ConnectToWifiNetworkUsingConnectData(
        static_cast<WifiNetwork*>(network));
  }
}

void NetworkLibraryImplCros::CallRequestWifiNetworkAndConnect(
    const std::string& ssid, ConnectionSecurity security) {
  // Asynchronously request service properties and call
  // WifiServiceUpdateAndConnect.
  CrosRequestHiddenWifiNetworkProperties(
      ssid.c_str(),
      SecurityToString(security),
      base::Bind(&WifiServiceUpdateAndConnect, this));
}

// static callback
void NetworkLibraryImplCros::VPNServiceUpdateAndConnect(
    void* object,
    const char* service_path,
    const base::DictionaryValue* properties) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (service_path && properties) {
    VLOG(1) << "Connecting to new VPN Service: " << service_path;
    Network* network =
        networklib->ParseNetwork(std::string(service_path), *properties);
    DCHECK_EQ(network->type(), TYPE_VPN);
    networklib->ConnectToVirtualNetworkUsingConnectData(
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
      service_name.c_str(),
      server_hostname.c_str(),
      ProviderTypeToString(provider_type),
      base::Bind(&VPNServiceUpdateAndConnect, this));
}

void NetworkLibraryImplCros::CallDeleteRememberedNetwork(
    const std::string& profile_path,
    const std::string& service_path) {
  CrosDeleteServiceFromProfile(
      profile_path.c_str(), service_path.c_str());
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
  CrosRequestChangePin(cellular->device_path().c_str(),
                       old_pin.c_str(), new_pin.c_str(),
                       PinOperationCallback, this);
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
  CrosRequestRequirePin(cellular->device_path().c_str(),
                        pin.c_str(), require_pin,
                        PinOperationCallback, this);
}

void NetworkLibraryImplCros::EnterPin(const std::string& pin) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling EnterPin method w/o cellular device.";
    return;
  }
  sim_operation_ = SIM_OPERATION_ENTER_PIN;
  CrosRequestEnterPin(cellular->device_path().c_str(),
                      pin.c_str(),
                      PinOperationCallback, this);
}

void NetworkLibraryImplCros::UnblockPin(const std::string& puk,
                                        const std::string& new_pin) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling UnblockPin method w/o cellular device.";
    return;
  }
  sim_operation_ = SIM_OPERATION_UNBLOCK_PIN;
  CrosRequestUnblockPin(cellular->device_path().c_str(),
                        puk.c_str(), new_pin.c_str(),
                        PinOperationCallback, this);
}

// static callback
void NetworkLibraryImplCros::PinOperationCallback(
    void* object,
    const char* path,
    NetworkMethodErrorType error,
    const char* error_message) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  PinOperationError pin_error;
  VLOG(1) << "PinOperationCallback, error: " << error
          << " error_msg: " << error_message;
  if (error == chromeos::NETWORK_METHOD_ERROR_NONE) {
    pin_error = PIN_ERROR_NONE;
    VLOG(1) << "Pin operation completed successfuly";
  } else {
    if (error_message &&
        (strcmp(error_message, flimflam::kErrorIncorrectPinMsg) == 0 ||
         strcmp(error_message, flimflam::kErrorPinRequiredMsg) == 0)) {
      pin_error = PIN_ERROR_INCORRECT_CODE;
    } else if (error_message &&
               strcmp(error_message, flimflam::kErrorPinBlockedMsg) == 0) {
      pin_error = PIN_ERROR_BLOCKED;
    } else {
      pin_error = PIN_ERROR_UNKNOWN;
      NOTREACHED() << "Unknown PIN error: " << error_message;
    }
  }
  networklib->NotifyPinOperationCompleted(pin_error);
}

void NetworkLibraryImplCros::RequestCellularScan() {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling RequestCellularScan method w/o cellular device.";
    return;
  }
  CrosProposeScan(cellular->device_path().c_str());
}

void NetworkLibraryImplCros::RequestCellularRegister(
    const std::string& network_id) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling CellularRegister method w/o cellular device.";
    return;
  }
  CrosRequestCellularRegister(cellular->device_path().c_str(),
                              network_id.c_str(),
                              CellularRegisterCallback,
                              this);
}

// static callback
void NetworkLibraryImplCros::CellularRegisterCallback(
    void* object,
    const char* path,
    NetworkMethodErrorType error,
    const char* error_message) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
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
  CrosSetNetworkDeviceProperty(cellular->device_path().c_str(),
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
  if (cellular_network())
    cellular_network()->RefreshDataPlansIfNeeded();
  // Make sure all Manager info is up to date. This will also update
  // remembered networks and visible services.
  CrosRequestNetworkManagerProperties(base::Bind(&NetworkManagerUpdate, this));
}

bool NetworkLibraryImplCros::GetWifiAccessPoints(
    WifiAccessPointVector* result) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DeviceNetworkList* network_list = CrosGetDeviceNetworkList();
  if (network_list == NULL)
    return false;
  result->clear();
  result->reserve(network_list->network_size);
  const base::Time now = base::Time::Now();
  for (size_t i = 0; i < network_list->network_size; ++i) {
    DCHECK(network_list->networks[i].address);
    DCHECK(network_list->networks[i].name);
    WifiAccessPoint ap;
    ap.mac_address = SafeString(network_list->networks[i].address);
    ap.name = SafeString(network_list->networks[i].name);
    ap.timestamp = now -
        base::TimeDelta::FromSeconds(network_list->networks[i].age_seconds);
    ap.signal_strength = network_list->networks[i].strength;
    ap.channel = network_list->networks[i].channel;
    result->push_back(ap);
  }
  CrosFreeDeviceNetworkList(network_list);
  return true;
}

void NetworkLibraryImplCros::DisconnectFromNetwork(const Network* network) {
  DCHECK(network);
  // Asynchronous disconnect request. Network state will be updated through
  // the network manager once disconnect completes.
  CrosRequestNetworkServiceDisconnect(network->service_path().c_str());
}

void NetworkLibraryImplCros::CallEnableNetworkDeviceType(
    ConnectionType device, bool enable) {
  busy_devices_ |= 1 << device;
  CrosRequestNetworkDeviceEnable(
      ConnectionTypeToString(device), enable);
}

void NetworkLibraryImplCros::CallRemoveNetwork(const Network* network) {
  const char* service_path = network->service_path().c_str();
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
  DCHECK(hardware_address);
  hardware_address->clear();
  NetworkIPConfigVector ipconfig_vector;
  if (!device_path.empty()) {
    IPConfigStatus* ipconfig_status = CrosListIPConfigs(device_path.c_str());
    if (ipconfig_status) {
      for (int i = 0; i < ipconfig_status->size; ++i) {
        IPConfig ipconfig = ipconfig_status->ips[i];
        ipconfig_vector.push_back(
            NetworkIPConfig(device_path, ipconfig.type, ipconfig.address,
                            ipconfig.netmask, ipconfig.gateway,
                            ipconfig.name_servers));
      }
      *hardware_address = ipconfig_status->hardware_address;
      CrosFreeIPConfigStatus(ipconfig_status);
    }
  }

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

  IPConfig* ipconfig_dhcp = NULL;
  IPConfig* ipconfig_static = NULL;

  IPConfigStatus* ipconfig_status =
      CrosListIPConfigs(ipconfig.device_path.c_str());
  if (ipconfig_status) {
    for (int i = 0; i < ipconfig_status->size; ++i) {
      if (ipconfig_status->ips[i].type == chromeos::IPCONFIG_TYPE_DHCP)
        ipconfig_dhcp = &ipconfig_status->ips[i];
      else if (ipconfig_status->ips[i].type == chromeos::IPCONFIG_TYPE_IPV4)
        ipconfig_static = &ipconfig_status->ips[i];
    }
  }

  IPConfigStatus* ipconfig_status2 = NULL;
  if (ipconfig.type == chromeos::IPCONFIG_TYPE_DHCP) {
    // If switching from static to dhcp, create new dhcp ip config.
    if (!ipconfig_dhcp)
      CrosAddIPConfig(ipconfig.device_path.c_str(),
                      chromeos::IPCONFIG_TYPE_DHCP);
    // User wants DHCP now. So delete the static ip config.
    if (ipconfig_static)
      CrosRemoveIPConfig(ipconfig_static);
  } else if (ipconfig.type == chromeos::IPCONFIG_TYPE_IPV4) {
    // If switching from dhcp to static, create new static ip config.
    if (!ipconfig_static) {
      CrosAddIPConfig(ipconfig.device_path.c_str(),
                      chromeos::IPCONFIG_TYPE_IPV4);
      // Now find the newly created IP config.
      ipconfig_status2 =
          CrosListIPConfigs(ipconfig.device_path.c_str());
      if (ipconfig_status2) {
        for (int i = 0; i < ipconfig_status2->size; ++i) {
          if (ipconfig_status2->ips[i].type == chromeos::IPCONFIG_TYPE_IPV4)
            ipconfig_static = &ipconfig_status2->ips[i];
        }
      }
    }
    if (ipconfig_static) {
      // Save any changed details.
      if (ipconfig.address != ipconfig_static->address) {
        base::StringValue value(ipconfig.address);
        CrosSetNetworkIPConfigProperty(ipconfig_static->path,
                                       flimflam::kAddressProperty, value);
      }
      if (ipconfig.netmask != ipconfig_static->netmask) {
        int prefixlen = ipconfig.GetPrefixLength();
        if (prefixlen == -1) {
          VLOG(1) << "IP config prefixlen is invalid for netmask "
                  << ipconfig.netmask;
        } else {
          base::FundamentalValue value(prefixlen);
          CrosSetNetworkIPConfigProperty(ipconfig_static->path,
                                         flimflam::kPrefixlenProperty, value);
        }
      }
      if (ipconfig.gateway != ipconfig_static->gateway) {
        base::StringValue value(ipconfig.gateway);
        CrosSetNetworkIPConfigProperty(ipconfig_static->path,
                                       flimflam::kGatewayProperty, value);
      }
      if (ipconfig.name_servers != ipconfig_static->name_servers) {
        base::StringValue value(ipconfig.name_servers);
        CrosSetNetworkIPConfigProperty(ipconfig_static->path,
                                       flimflam::kNameServersProperty, value);
      }
      // Remove dhcp ip config if there is one.
      if (ipconfig_dhcp)
        CrosRemoveIPConfig(ipconfig_dhcp);
    }
  }

  if (ipconfig_status)
    CrosFreeIPConfigStatus(ipconfig_status);
  if (ipconfig_status2)
    CrosFreeIPConfigStatus(ipconfig_status2);
}

/////////////////////////////////////////////////////////////////////////////
// Network Manager functions.

// static
void NetworkLibraryImplCros::NetworkManagerStatusChangedHandler(
    void* object, const char* path, const char* key, const GValue* gvalue) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  scoped_ptr<Value> value(ConvertGValueToValue(gvalue));
  networklib->NetworkManagerStatusChanged(key, value.get());
}

// This processes all Manager update messages.
void NetworkLibraryImplCros::NetworkManagerStatusChanged(
    const char* key, const Value* value) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::TimeTicks start = base::TimeTicks::Now();
  if (!key)
    return;
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

// static
void NetworkLibraryImplCros::NetworkManagerUpdate(
    void* object,
    const char* manager_path,
    const base::DictionaryValue* properties) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (!properties) {
    LOG(ERROR) << "Error retrieving manager properties: " << manager_path;
    return;
  }
  VLOG(1) << "Received NetworkManagerUpdate.";
  networklib->ParseNetworkManager(*properties);
}

void NetworkLibraryImplCros::ParseNetworkManager(const DictionaryValue& dict) {
  for (DictionaryValue::key_iterator iter = dict.begin_keys();
       iter != dict.end_keys(); ++iter) {
    const std::string& key = *iter;
    Value* value;
    bool res = dict.GetWithoutPathExpansion(key, &value);
    CHECK(res);
    NetworkManagerStatusChanged(key.c_str(), value);
  }
  // If there is no Profiles entry, request remembered networks here.
  if (!dict.HasKey(flimflam::kProfilesProperty))
    RequestRememberedNetworksUpdate();
}

// static
void NetworkLibraryImplCros::DataPlanUpdateHandler(
    void* object,
    const char* modem_service_path,
    const chromeos::CellularDataPlanList* data_plan_list) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (modem_service_path && data_plan_list) {
    // Copy contents of |data_plan_list| from libcros to |data_plan_vector|.
    CellularDataPlanVector* data_plan_vector = new CellularDataPlanVector;
    for (size_t i = 0; i < data_plan_list->plans_size; ++i) {
      const CellularDataPlanInfo* info(data_plan_list->GetCellularDataPlan(i));
      CellularDataPlan* plan = new CellularDataPlan(*info);
      data_plan_vector->push_back(plan);
      VLOG(2) << " Plan: " << plan->GetPlanDesciption()
              << " : " << plan->GetDataRemainingDesciption();
    }
    // |data_plan_vector| will become owned by networklib.
    networklib->UpdateCellularDataPlan(std::string(modem_service_path),
                                       data_plan_vector);
  }
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
      CrosRequestNetworkServiceProperties(service_path.c_str(),
                                          base::Bind(&NetworkServiceUpdate,
                                                     this));
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
          service_path.c_str(),
          base::Bind(&NetworkServiceUpdate, this));
    }
  }
}

// static
void NetworkLibraryImplCros::NetworkServiceUpdate(
    void* object,
    const char* service_path,
    const base::DictionaryValue* properties) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (service_path) {
    if (!properties)
      return;  // Network no longer in visible list, ignore.
    networklib->ParseNetwork(std::string(service_path), *properties);
  }
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
        profile.path.c_str(), base::Bind(&ProfileUpdate, this));
  }
}

// static
void NetworkLibraryImplCros::ProfileUpdate(
    void* object,
    const char* profile_path,
    const base::DictionaryValue* properties) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (!properties) {
    LOG(ERROR) << "Error retrieving profile: " << profile_path;
    return;
  }
  VLOG(1) << "Received ProfileUpdate for: " << profile_path;
  ListValue* entries(NULL);
  properties->GetList(flimflam::kEntriesProperty, &entries);
  DCHECK(entries);
  networklib->UpdateRememberedServiceList(profile_path, entries);
}

void NetworkLibraryImplCros::UpdateRememberedServiceList(
    const char* profile_path, const ListValue* profile_entries) {
  DCHECK(profile_path);
  VLOG(1) << "UpdateRememberedServiceList for path: " << profile_path;
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
        service_path.c_str(),
        base::Bind(&RememberedNetworkServiceUpdate, this));
  }
}

// static
void NetworkLibraryImplCros::RememberedNetworkServiceUpdate(
    void* object,
    const char* service_path,
    const base::DictionaryValue* properties) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (service_path) {
    if (!properties) {
      // Remembered network no longer exists.
      networklib->DeleteRememberedNetwork(std::string(service_path));
    } else {
      networklib->ParseRememberedNetwork(
          std::string(service_path), *properties);
    }
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
          vpn->name().c_str(),
          vpn->server_hostname().c_str(),
          provider_type.c_str(),
          base::Bind(&NetworkServiceUpdate, this));
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
          device_path.c_str(),
          base::Bind(&NetworkDeviceUpdate, this));
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

// static
void NetworkLibraryImplCros::NetworkDeviceUpdate(
    void* object,
    const char* device_path,
    const base::DictionaryValue* properties) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (device_path) {
    if (!properties) {
      // device no longer exists.
      networklib->DeleteDevice(std::string(device_path));
    } else {
      networklib->ParseNetworkDevice(std::string(device_path), *properties);
    }
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
