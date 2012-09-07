// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_network_functions.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_tokenizer.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/sms_watcher.h"
#include "chromeos/dbus/cashew_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_network_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Class to watch network manager's properties without Libcros.
class NetworkManagerPropertiesWatcher : public CrosNetworkWatcher {
 public:
  NetworkManagerPropertiesWatcher(
      const NetworkPropertiesWatcherCallback& callback) {
    DBusThreadManager::Get()->GetShillManagerClient()->
        SetPropertyChangedHandler(base::Bind(callback,
                                             flimflam::kFlimflamServicePath));
  }
  virtual ~NetworkManagerPropertiesWatcher() {
    DBusThreadManager::Get()->GetShillManagerClient()->
        ResetPropertyChangedHandler();
  }
};

// Class to watch network service's properties without Libcros.
class NetworkServicePropertiesWatcher : public CrosNetworkWatcher {
 public:
  NetworkServicePropertiesWatcher(
      const NetworkPropertiesWatcherCallback& callback,
      const std::string& service_path) : service_path_(service_path) {
    DBusThreadManager::Get()->GetShillServiceClient()->
        SetPropertyChangedHandler(dbus::ObjectPath(service_path),
                                  base::Bind(callback, service_path));
  }
  virtual ~NetworkServicePropertiesWatcher() {
    DBusThreadManager::Get()->GetShillServiceClient()->
        ResetPropertyChangedHandler(dbus::ObjectPath(service_path_));
  }

 private:
  std::string service_path_;
};

// Class to watch network device's properties without Libcros.
class NetworkDevicePropertiesWatcher : public CrosNetworkWatcher {
 public:
  NetworkDevicePropertiesWatcher(
      const NetworkPropertiesWatcherCallback& callback,
      const std::string& device_path) : device_path_(device_path) {
    DBusThreadManager::Get()->GetShillDeviceClient()->
        SetPropertyChangedHandler(dbus::ObjectPath(device_path),
                                  base::Bind(callback, device_path));
  }
  virtual ~NetworkDevicePropertiesWatcher() {
    DBusThreadManager::Get()->GetShillDeviceClient()->
        ResetPropertyChangedHandler(dbus::ObjectPath(device_path_));
  }

 private:
  std::string device_path_;
};

// Converts a string to a CellularDataPlanType.
CellularDataPlanType ParseCellularDataPlanType(const std::string& type) {
  if (type == cashew::kCellularDataPlanUnlimited)
    return CELLULAR_DATA_PLAN_UNLIMITED;
  if (type == cashew::kCellularDataPlanMeteredPaid)
    return CELLULAR_DATA_PLAN_METERED_PAID;
  if (type == cashew::kCellularDataPlanMeteredBase)
    return CELLULAR_DATA_PLAN_METERED_BASE;
  return CELLULAR_DATA_PLAN_UNKNOWN;
}

// Gets a string property from dictionary.
bool GetStringProperty(const base::DictionaryValue& dictionary,
                       const std::string& key,
                       std::string* out) {
  const bool result = dictionary.GetStringWithoutPathExpansion(key, out);
  LOG_IF(WARNING, !result) << "Cannnot get property " << key;
  return result;
}

// Gets an int64 property from dictionary.
bool GetInt64Property(const base::DictionaryValue& dictionary,
                      const std::string& key,
                      int64* out) {
  // Int64 value is stored as a double because it cannot be fitted in int32.
  double value_double = 0;
  const bool result = dictionary.GetDoubleWithoutPathExpansion(key,
                                                               &value_double);
  if (result)
    *out = value_double;
  else
    LOG(WARNING) << "Cannnot get property " << key;
  return result;
}

// Gets a base::Time property from dictionary.
bool GetTimeProperty(const base::DictionaryValue& dictionary,
                     const std::string& key,
                     base::Time* out) {
  int64 value_int64 = 0;
  if (!GetInt64Property(dictionary, key, &value_int64))
    return false;
  *out = base::Time::FromInternalValue(value_int64);
  return true;
}

// Class to watch data plan update without Libcros.
class DataPlanUpdateWatcher : public CrosNetworkWatcher {
 public:
  explicit DataPlanUpdateWatcher(const DataPlanUpdateWatcherCallback& callback)
      : callback_(callback) {
    DBusThreadManager::Get()->GetCashewClient()->SetDataPlansUpdateHandler(
        base::Bind(&DataPlanUpdateWatcher::OnDataPlansUpdate,
                   base::Unretained(this)));
  }
  virtual ~DataPlanUpdateWatcher() {
    DBusThreadManager::Get()->GetCashewClient()->ResetDataPlansUpdateHandler();
  }

 private:
  void OnDataPlansUpdate(const std::string& service,
                         const base::ListValue& data_plans) {
    CellularDataPlanVector* data_plan_vector = new CellularDataPlanVector;
    for (size_t i = 0; i != data_plans.GetSize(); ++i) {
      const base::DictionaryValue* data_plan = NULL;
      if (!data_plans.GetDictionary(i, &data_plan)) {
        LOG(ERROR) << "data_plans["  << i << "] is not a dictionary.";
        continue;
      }
      CellularDataPlan* plan = new CellularDataPlan;
      // Plan name.
      GetStringProperty(*data_plan, cashew::kCellularPlanNameProperty,
                        &plan->plan_name);
      // Plan type.
      std::string plan_type_string;
      GetStringProperty(*data_plan, cashew::kCellularPlanTypeProperty,
                        &plan_type_string);
      plan->plan_type = ParseCellularDataPlanType(plan_type_string);
      // Update time.
      GetTimeProperty(*data_plan, cashew::kCellularPlanUpdateTimeProperty,
                      &plan->update_time);
      // Start time.
      GetTimeProperty(*data_plan, cashew::kCellularPlanStartProperty,
                      &plan->plan_start_time);
      // End time.
      GetTimeProperty(*data_plan, cashew::kCellularPlanEndProperty,
                      &plan->plan_end_time);
      // Data bytes.
      GetInt64Property(*data_plan, cashew::kCellularPlanDataBytesProperty,
                       &plan->plan_data_bytes);
      // Bytes used.
      GetInt64Property(*data_plan, cashew::kCellularDataBytesUsedProperty,
                       &plan->data_bytes_used);
      data_plan_vector->push_back(plan);
    }
    callback_.Run(service, data_plan_vector);
  }

  DataPlanUpdateWatcherCallback callback_;
};

// Does nothing. Used as a callback.
void DoNothing(DBusMethodCallStatus call_status) {}

// A callback used to implement CrosRequest*Properties functions.
void RunCallbackWithDictionaryValue(const NetworkPropertiesCallback& callback,
                                    const std::string& path,
                                    DBusMethodCallStatus call_status,
                                    const base::DictionaryValue& value) {
  callback.Run(path, call_status == DBUS_METHOD_CALL_SUCCESS ? &value : NULL);
}

// Used as a callback for ShillManagerClient::GetService
void OnGetService(const NetworkPropertiesCallback& callback,
                  DBusMethodCallStatus call_status,
                  const dbus::ObjectPath& service_path) {
  if (call_status == DBUS_METHOD_CALL_SUCCESS) {
    VLOG(1) << "OnGetServiceService: " << service_path.value();
    DBusThreadManager::Get()->GetShillServiceClient()->GetProperties(
        service_path, base::Bind(&RunCallbackWithDictionaryValue,
                                 callback,
                                 service_path.value()));
  }
}

// A callback used to call a NetworkOperationCallback on error.
void OnNetworkActionError(const NetworkOperationCallback& callback,
                          const std::string& path,
                          const std::string& error_name,
                          const std::string& error_message) {
  if (error_name.empty())
    callback.Run(path, NETWORK_METHOD_ERROR_LOCAL, "");
  else
    callback.Run(path, NETWORK_METHOD_ERROR_REMOTE, error_message);
}

IPConfigType ParseIPConfigType(const std::string& type) {
  if (type == flimflam::kTypeIPv4)
    return IPCONFIG_TYPE_IPV4;
  if (type == flimflam::kTypeIPv6)
    return IPCONFIG_TYPE_IPV6;
  if (type == flimflam::kTypeDHCP)
    return IPCONFIG_TYPE_DHCP;
  if (type == flimflam::kTypeBOOTP)
    return IPCONFIG_TYPE_BOOTP;
  if (type == flimflam::kTypeZeroConf)
    return IPCONFIG_TYPE_ZEROCONF;
  if (type == flimflam::kTypeDHCP6)
    return IPCONFIG_TYPE_DHCP6;
  if (type == flimflam::kTypePPP)
    return IPCONFIG_TYPE_PPP;
  return IPCONFIG_TYPE_UNKNOWN;
}

// Converts a list of name servers to a string.
std::string ConvertNameSerersListToString(const base::ListValue& name_servers) {
  std::string result;
  for (size_t i = 0; i != name_servers.GetSize(); ++i) {
    std::string name_server;
    if (!name_servers.GetString(i, &name_server)) {
      LOG(ERROR) << "name_servers[" << i << "] is not a string.";
      continue;
    }
    if (!result.empty())
      result += ",";
    result += name_server;
  }
  return result;
}

// Gets NetworkIPConfigVector populated with data from a
// given DBus object path.
//
// returns true on success.
bool ParseIPConfig(const std::string& device_path,
                   const std::string& ipconfig_path,
                   NetworkIPConfigVector* ipconfig_vector) {
  ShillIPConfigClient* ipconfig_client =
      DBusThreadManager::Get()->GetShillIPConfigClient();
  // TODO(hashimoto): Remove this blocking D-Bus method call. crosbug.com/29902
  scoped_ptr<base::DictionaryValue> properties(
      ipconfig_client->CallGetPropertiesAndBlock(
          dbus::ObjectPath(ipconfig_path)));
  if (!properties.get())
    return false;

  std::string type_string;
  properties->GetStringWithoutPathExpansion(flimflam::kMethodProperty,
                                            &type_string);
  std::string address;
  properties->GetStringWithoutPathExpansion(flimflam::kAddressProperty,
                                            &address);
  int32 prefix_len = 0;
  properties->GetIntegerWithoutPathExpansion(flimflam::kPrefixlenProperty,
                                             &prefix_len);
  std::string gateway;
  properties->GetStringWithoutPathExpansion(flimflam::kGatewayProperty,
                                            &gateway);
  base::ListValue* name_servers = NULL;
  std::string name_servers_string;
  // store nameservers as a comma delimited list
  if (properties->GetListWithoutPathExpansion(flimflam::kNameServersProperty,
                                              &name_servers)) {
    name_servers_string = ConvertNameSerersListToString(*name_servers);
  } else {
    LOG(ERROR) << "Cannot get name servers.";
  }
  ipconfig_vector->push_back(
      NetworkIPConfig(device_path,
                      ParseIPConfigType(type_string),
                      address,
                      CrosPrefixLengthToNetmask(prefix_len),
                      gateway,
                      name_servers_string));
  return true;
}

}  // namespace

SMS::SMS()
    : validity(0),
      msgclass(0) {
}

SMS::~SMS() {}

bool CrosActivateCellularModem(const std::string& service_path,
                               const std::string& carrier) {
  return DBusThreadManager::Get()->GetShillServiceClient()->
      CallActivateCellularModemAndBlock(dbus::ObjectPath(service_path),
                                        carrier);
}

void CrosSetNetworkServiceProperty(const std::string& service_path,
                                   const std::string& property,
                                   const base::Value& value) {
  DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
      dbus::ObjectPath(service_path), property, value,
      base::Bind(&DoNothing));
}

void CrosClearNetworkServiceProperty(const std::string& service_path,
                                     const std::string& property) {
  DBusThreadManager::Get()->GetShillServiceClient()->ClearProperty(
      dbus::ObjectPath(service_path), property, base::Bind(&DoNothing));
}

void CrosSetNetworkDeviceProperty(const std::string& device_path,
                                  const std::string& property,
                                  const base::Value& value) {
  DBusThreadManager::Get()->GetShillDeviceClient()->SetProperty(
      dbus::ObjectPath(device_path), property, value, base::Bind(&DoNothing));
}

void CrosSetNetworkIPConfigProperty(const std::string& ipconfig_path,
                                    const std::string& property,
                                    const base::Value& value) {
  DBusThreadManager::Get()->GetShillIPConfigClient()->SetProperty(
      dbus::ObjectPath(ipconfig_path), property, value,
      base::Bind(&DoNothing));
}

void CrosSetNetworkManagerProperty(const std::string& property,
                                   const base::Value& value) {
  DBusThreadManager::Get()->GetShillManagerClient()->SetProperty(
      property, value, base::Bind(&DoNothing));
}

void CrosDeleteServiceFromProfile(const std::string& profile_path,
                                  const std::string& service_path) {
  DBusThreadManager::Get()->GetShillProfileClient()->DeleteEntry(
      dbus::ObjectPath(profile_path), service_path, base::Bind(&DoNothing));
}

void CrosRequestCellularDataPlanUpdate(const std::string& modem_service_path) {
  DBusThreadManager::Get()->GetCashewClient()->RequestDataPlansUpdate(
      modem_service_path);
}

CrosNetworkWatcher* CrosMonitorNetworkManagerProperties(
    const NetworkPropertiesWatcherCallback& callback) {
  return new NetworkManagerPropertiesWatcher(callback);
}

CrosNetworkWatcher* CrosMonitorNetworkServiceProperties(
    const NetworkPropertiesWatcherCallback& callback,
    const std::string& service_path) {
  return new NetworkServicePropertiesWatcher(callback, service_path);
}

CrosNetworkWatcher* CrosMonitorNetworkDeviceProperties(
    const NetworkPropertiesWatcherCallback& callback,
    const std::string& device_path) {
  return new NetworkDevicePropertiesWatcher(callback, device_path);
}

CrosNetworkWatcher* CrosMonitorCellularDataPlan(
    const DataPlanUpdateWatcherCallback& callback) {
  return new DataPlanUpdateWatcher(callback);
}

CrosNetworkWatcher* CrosMonitorSMS(const std::string& modem_device_path,
                                   MonitorSMSCallback callback) {
  return new SMSWatcher(modem_device_path, callback);
}

void CrosRequestNetworkServiceConnect(
    const std::string& service_path,
    const NetworkOperationCallback& callback) {
  DBusThreadManager::Get()->GetShillServiceClient()->Connect(
      dbus::ObjectPath(service_path),
      base::Bind(callback, service_path, NETWORK_METHOD_ERROR_NONE,
                 std::string()),
      base::Bind(&OnNetworkActionError, callback, service_path));
}

void CrosRequestNetworkManagerProperties(
    const NetworkPropertiesCallback& callback) {
  DBusThreadManager::Get()->GetShillManagerClient()->GetProperties(
      base::Bind(&RunCallbackWithDictionaryValue,
                 callback,
                 flimflam::kFlimflamServicePath));
}

void CrosRequestNetworkServiceProperties(
    const std::string& service_path,
    const NetworkPropertiesCallback& callback) {
  DBusThreadManager::Get()->GetShillServiceClient()->GetProperties(
      dbus::ObjectPath(service_path),
      base::Bind(&RunCallbackWithDictionaryValue, callback, service_path));
}

void CrosRequestNetworkDeviceProperties(
    const std::string& device_path,
    const NetworkPropertiesCallback& callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->GetProperties(
      dbus::ObjectPath(device_path),
      base::Bind(&RunCallbackWithDictionaryValue, callback, device_path));
}

void CrosRequestNetworkProfileProperties(
    const std::string& profile_path,
    const NetworkPropertiesCallback& callback) {
  DBusThreadManager::Get()->GetShillProfileClient()->GetProperties(
      dbus::ObjectPath(profile_path),
      base::Bind(&RunCallbackWithDictionaryValue, callback, profile_path));
}

void CrosRequestNetworkProfileEntryProperties(
    const std::string& profile_path,
    const std::string& profile_entry_path,
    const NetworkPropertiesCallback& callback) {
  DBusThreadManager::Get()->GetShillProfileClient()->GetEntry(
      dbus::ObjectPath(profile_path),
      profile_entry_path,
      base::Bind(&RunCallbackWithDictionaryValue,
                 callback,
                 profile_entry_path));
}

void CrosRequestHiddenWifiNetworkProperties(
    const std::string& ssid,
    const std::string& security,
    const NetworkPropertiesCallback& callback) {
  base::DictionaryValue properties;
  properties.SetWithoutPathExpansion(
      flimflam::kModeProperty,
      base::Value::CreateStringValue(flimflam::kModeManaged));
  properties.SetWithoutPathExpansion(
      flimflam::kTypeProperty,
      base::Value::CreateStringValue(flimflam::kTypeWifi));
  properties.SetWithoutPathExpansion(
      flimflam::kSSIDProperty,
      base::Value::CreateStringValue(ssid));
  properties.SetWithoutPathExpansion(
      flimflam::kSecurityProperty,
      base::Value::CreateStringValue(security));
  // shill.Manger.GetService() will apply the property changes in
  // |properties| and return a new or existing service to OnGetService().
  // OnGetService will then call GetProperties which will then call callback.
  DBusThreadManager::Get()->GetShillManagerClient()->GetService(
      properties, base::Bind(&OnGetService, callback));
}

void CrosRequestVirtualNetworkProperties(
    const std::string& service_name,
    const std::string& server_hostname,
    const std::string& provider_type,
    const NetworkPropertiesCallback& callback) {
  base::DictionaryValue properties;
  properties.SetWithoutPathExpansion(
      flimflam::kTypeProperty,
      base::Value::CreateStringValue(flimflam::kTypeVPN));
  properties.SetWithoutPathExpansion(
      flimflam::kProviderNameProperty,
      base::Value::CreateStringValue(service_name));
  properties.SetWithoutPathExpansion(
      flimflam::kProviderHostProperty,
      base::Value::CreateStringValue(server_hostname));
  properties.SetWithoutPathExpansion(
      flimflam::kProviderTypeProperty,
      base::Value::CreateStringValue(provider_type));
  // The actual value of Domain does not matter, so just use service_name.
  properties.SetWithoutPathExpansion(
      flimflam::kVPNDomainProperty,
      base::Value::CreateStringValue(service_name));

  // shill.Manger.GetService() will apply the property changes in
  // |properties| and pass a new or existing service to OnGetService().
  // OnGetService will then call GetProperties which will then call callback.
  DBusThreadManager::Get()->GetShillManagerClient()->GetService(
      properties, base::Bind(&OnGetService, callback));
}

void CrosRequestNetworkServiceDisconnect(const std::string& service_path) {
  DBusThreadManager::Get()->GetShillServiceClient()->Disconnect(
      dbus::ObjectPath(service_path), base::Bind(&DoNothing));
}

void CrosRequestRemoveNetworkService(const std::string& service_path) {
  DBusThreadManager::Get()->GetShillServiceClient()->Remove(
      dbus::ObjectPath(service_path), base::Bind(&DoNothing));
}

void CrosRequestNetworkScan(const std::string& network_type) {
  DBusThreadManager::Get()->GetShillManagerClient()->RequestScan(
      network_type, base::Bind(&DoNothing));
}

void CrosRequestNetworkDeviceEnable(const std::string& network_type,
                                    bool enable) {
  if (enable) {
    DBusThreadManager::Get()->GetShillManagerClient()->EnableTechnology(
        network_type, base::Bind(&DoNothing));
  } else {
    DBusThreadManager::Get()->GetShillManagerClient()->DisableTechnology(
        network_type, base::Bind(&DoNothing));
  }
}

void CrosRequestRequirePin(const std::string& device_path,
                           const std::string& pin,
                           bool enable,
                           const NetworkOperationCallback& callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->RequirePin(
      dbus::ObjectPath(device_path), pin, enable,
      base::Bind(callback, device_path, NETWORK_METHOD_ERROR_NONE,
                 std::string()),
      base::Bind(&OnNetworkActionError, callback, device_path));
}

void CrosRequestEnterPin(const std::string& device_path,
                         const std::string& pin,
                         const NetworkOperationCallback& callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->EnterPin(
      dbus::ObjectPath(device_path), pin,
      base::Bind(callback, device_path, NETWORK_METHOD_ERROR_NONE,
                 std::string()),
      base::Bind(&OnNetworkActionError, callback, device_path));
}

void CrosRequestUnblockPin(const std::string& device_path,
                           const std::string& unblock_code,
                           const std::string& pin,
                           const NetworkOperationCallback& callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->UnblockPin(
      dbus::ObjectPath(device_path), unblock_code, pin,
      base::Bind(callback, device_path, NETWORK_METHOD_ERROR_NONE,
                 std::string()),
      base::Bind(&OnNetworkActionError, callback, device_path));
}

void CrosRequestChangePin(const std::string& device_path,
                          const std::string& old_pin,
                          const std::string& new_pin,
                          const NetworkOperationCallback& callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->ChangePin(
      dbus::ObjectPath(device_path), old_pin, new_pin,
      base::Bind(callback, device_path, NETWORK_METHOD_ERROR_NONE,
                 std::string()),
      base::Bind(&OnNetworkActionError, callback, device_path));
}

void CrosProposeScan(const std::string& device_path) {
  DBusThreadManager::Get()->GetShillDeviceClient()->ProposeScan(
      dbus::ObjectPath(device_path), base::Bind(&DoNothing));
}

void CrosRequestCellularRegister(const std::string& device_path,
                                 const std::string& network_id,
                                 const NetworkOperationCallback& callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->Register(
      dbus::ObjectPath(device_path), network_id,
      base::Bind(callback, device_path, NETWORK_METHOD_ERROR_NONE,
                 std::string()),
      base::Bind(&OnNetworkActionError, callback, device_path));
}

bool CrosSetOfflineMode(bool offline) {
  base::FundamentalValue value(offline);
  DBusThreadManager::Get()->GetShillManagerClient()->SetProperty(
      flimflam::kOfflineModeProperty, value, base::Bind(&DoNothing));
  return true;
}

bool CrosListIPConfigs(const std::string& device_path,
                       NetworkIPConfigVector* ipconfig_vector,
                       std::vector<std::string>* ipconfig_paths,
                       std::string* hardware_address) {
  if (hardware_address)
    hardware_address->clear();
  const dbus::ObjectPath device_object_path(device_path);
  ShillDeviceClient* device_client =
      DBusThreadManager::Get()->GetShillDeviceClient();
  // TODO(hashimoto): Remove this blocking D-Bus method call.
  // crosbug.com/29902
  scoped_ptr<base::DictionaryValue> properties(
      device_client->CallGetPropertiesAndBlock(device_object_path));
  if (!properties.get())
    return false;

  ListValue* ips = NULL;
  if (!properties->GetListWithoutPathExpansion(
          flimflam::kIPConfigsProperty, &ips))
    return false;

  for (size_t i = 0; i < ips->GetSize(); i++) {
    std::string ipconfig_path;
    if (!ips->GetString(i, &ipconfig_path)) {
      LOG(WARNING) << "Found NULL ip for device " << device_path;
      continue;
    }
    ParseIPConfig(device_path, ipconfig_path, ipconfig_vector);
    if (ipconfig_paths)
      ipconfig_paths->push_back(ipconfig_path);
  }
  // Store the hardware address as well.
  if (hardware_address)
    properties->GetStringWithoutPathExpansion(flimflam::kAddressProperty,
                                              hardware_address);
  return true;
}

bool CrosAddIPConfig(const std::string& device_path, IPConfigType type) {
  std::string type_str;
  switch (type) {
    case IPCONFIG_TYPE_IPV4:
      type_str = flimflam::kTypeIPv4;
      break;
    case IPCONFIG_TYPE_IPV6:
      type_str = flimflam::kTypeIPv6;
      break;
    case IPCONFIG_TYPE_DHCP:
      type_str = flimflam::kTypeDHCP;
      break;
    case IPCONFIG_TYPE_BOOTP:
      type_str = flimflam::kTypeBOOTP;
      break;
    case IPCONFIG_TYPE_ZEROCONF:
      type_str = flimflam::kTypeZeroConf;
      break;
    case IPCONFIG_TYPE_DHCP6:
      type_str = flimflam::kTypeDHCP6;
      break;
    case IPCONFIG_TYPE_PPP:
      type_str = flimflam::kTypePPP;
      break;
    default:
      return false;
  };
  const dbus::ObjectPath result =
      DBusThreadManager::Get()->GetShillDeviceClient()->
      CallAddIPConfigAndBlock(dbus::ObjectPath(device_path), type_str);
  if (result.value().empty()) {
    LOG(ERROR) << "Add IPConfig failed for device path " << device_path
               << " and type " << type_str;
    return false;
  }
  return true;
}

bool CrosRemoveIPConfig(const std::string& ipconfig_path) {
  return DBusThreadManager::Get()->GetShillIPConfigClient()->
      CallRemoveAndBlock(dbus::ObjectPath(ipconfig_path));
}

void CrosRequestIPConfigRefresh(const std::string& ipconfig_path) {
  DBusThreadManager::Get()->GetShillIPConfigClient()->Refresh(
      dbus::ObjectPath(ipconfig_path),
      base::Bind(&DoNothing));
}

bool CrosGetWifiAccessPoints(WifiAccessPointVector* result) {
  scoped_ptr<base::DictionaryValue> manager_properties(
      DBusThreadManager::Get()->GetShillManagerClient()->
      CallGetPropertiesAndBlock());
  if (!manager_properties.get()) {
    LOG(WARNING) << "Couldn't read managers's properties";
    return false;
  }

  base::ListValue* devices = NULL;
  if (!manager_properties->GetListWithoutPathExpansion(
          flimflam::kDevicesProperty, &devices)) {
    LOG(WARNING) << flimflam::kDevicesProperty << " property not found";
    return false;
  }
  const base::Time now = base::Time::Now();
  bool found_at_least_one_device = false;
  result->clear();
  for (size_t i = 0; i < devices->GetSize(); i++) {
    std::string device_path;
    if (!devices->GetString(i, &device_path)) {
      LOG(WARNING) << "Couldn't get devices[" << i << "]";
      continue;
    }
    scoped_ptr<base::DictionaryValue> device_properties(
        DBusThreadManager::Get()->GetShillDeviceClient()->
        CallGetPropertiesAndBlock(dbus::ObjectPath(device_path)));
    if (!device_properties.get()) {
      LOG(WARNING) << "Couldn't read device's properties " << device_path;
      continue;
    }

    base::ListValue* networks = NULL;
    if (!device_properties->GetListWithoutPathExpansion(
            flimflam::kNetworksProperty, &networks))
      continue;  // Some devices do not list networks, e.g. ethernet.

    base::Value* device_powered_value = NULL;
    bool device_powered = false;
    if (device_properties->GetWithoutPathExpansion(
            flimflam::kPoweredProperty, &device_powered_value) &&
        device_powered_value->GetAsBoolean(&device_powered) &&
        !device_powered)
      continue;  // Skip devices that are not powered up.

    int scan_interval = 0;
    device_properties->GetIntegerWithoutPathExpansion(
        flimflam::kScanIntervalProperty, &scan_interval);

    found_at_least_one_device = true;
    for (size_t j = 0; j < networks->GetSize(); j++) {
      std::string network_path;
      if (!networks->GetString(j, &network_path)) {
        LOG(WARNING) << "Couldn't get networks[" << j << "]";
        continue;
      }

      scoped_ptr<base::DictionaryValue> network_properties(
          DBusThreadManager::Get()->GetShillNetworkClient()->
          CallGetPropertiesAndBlock(dbus::ObjectPath(network_path)));
      if (!network_properties.get()) {
        LOG(WARNING) << "Couldn't read network's properties " << network_path;
        continue;
      }

      // Using the scan interval as a proxy for approximate age.
      // TODO(joth): Replace with actual age, when available from dbus.
      const int age_seconds = scan_interval;
      WifiAccessPoint ap;
      network_properties->GetStringWithoutPathExpansion(
          flimflam::kAddressProperty, &ap.mac_address);
      network_properties->GetStringWithoutPathExpansion(
          flimflam::kNameProperty, &ap.name);
      ap.timestamp = now - base::TimeDelta::FromSeconds(age_seconds);
      network_properties->GetIntegerWithoutPathExpansion(
          flimflam::kSignalStrengthProperty, &ap.signal_strength);
      network_properties->GetIntegerWithoutPathExpansion(
          flimflam::kWifiChannelProperty, &ap.channel);
      result->push_back(ap);
    }
  }
  if (!found_at_least_one_device)
    return false;  // No powered device found that has a 'Networks' array.
  return true;
}

void CrosConfigureService(const base::DictionaryValue& properties) {
  DBusThreadManager::Get()->GetShillManagerClient()->ConfigureService(
      properties, base::Bind(&DoNothing));
}

std::string CrosPrefixLengthToNetmask(int32 prefix_length) {
  std::string netmask;
  // Return the empty string for invalid inputs.
  if (prefix_length < 0 || prefix_length > 32)
    return netmask;
  for (int i = 0; i < 4; i++) {
    int remainder = 8;
    if (prefix_length >= 8) {
      prefix_length -= 8;
    } else {
      remainder = prefix_length;
      prefix_length = 0;
    }
    if (i > 0)
      netmask += ".";
    int value = remainder == 0 ? 0 :
        ((2L << (remainder - 1)) - 1) << (8 - remainder);
    netmask += StringPrintf("%d", value);
  }
  return netmask;
}

int32 CrosNetmaskToPrefixLength(const std::string& netmask) {
  int count = 0;
  int prefix_length = 0;
  StringTokenizer t(netmask, ".");
  while (t.GetNext()) {
    // If there are more than 4 numbers, then it's invalid.
    if (count == 4)
      return -1;

    std::string token = t.token();
    // If we already found the last mask and the current one is not
    // "0" then the netmask is invalid. For example, 255.224.255.0
    if (prefix_length / 8 != count) {
      if (token != "0")
        return -1;
    } else if (token == "255") {
      prefix_length += 8;
    } else if (token == "254") {
      prefix_length += 7;
    } else if (token == "252") {
      prefix_length += 6;
    } else if (token == "248") {
      prefix_length += 5;
    } else if (token == "240") {
      prefix_length += 4;
    } else if (token == "224") {
      prefix_length += 3;
    } else if (token == "192") {
      prefix_length += 2;
    } else if (token == "128") {
      prefix_length += 1;
    } else if (token == "0") {
      prefix_length += 0;
    } else {
      // mask is not a valid number.
      return -1;
    }
    count++;
  }
  if (count < 4)
    return -1;
  return prefix_length;
}

}  // namespace chromeos
