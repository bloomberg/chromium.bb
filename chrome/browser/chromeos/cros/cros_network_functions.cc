// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_network_functions.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/gvalue_util.h"
#include "chrome/browser/chromeos/cros/sms_watcher.h"
#include "chromeos/dbus/cashew_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/flimflam_device_client.h"
#include "chromeos/dbus/flimflam_ipconfig_client.h"
#include "chromeos/dbus/flimflam_manager_client.h"
#include "chromeos/dbus/flimflam_network_client.h"
#include "chromeos/dbus/flimflam_profile_client.h"
#include "chromeos/dbus/flimflam_service_client.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Used as a callback for chromeos::MonitorNetwork*Properties.
void OnNetworkPropertiesChanged(void* object,
                                const char* path,
                                const char* key,
                                const GValue* gvalue);

// Class to watch network manager's properties with Libcros.
class CrosNetworkManagerPropertiesWatcher : public CrosNetworkWatcher {
 public:
  CrosNetworkManagerPropertiesWatcher(
      const NetworkPropertiesWatcherCallback& callback)
      : callback_(callback),
        monitor_(chromeos::MonitorNetworkManagerProperties(
            &OnNetworkPropertiesChanged, &callback_)) {}
  virtual ~CrosNetworkManagerPropertiesWatcher() {
    chromeos::DisconnectNetworkPropertiesMonitor(monitor_);
  }

 private:
  NetworkPropertiesWatcherCallback callback_;
  NetworkPropertiesMonitor monitor_;
};

// Class to watch network manager's properties without Libcros.
class NetworkManagerPropertiesWatcher : public CrosNetworkWatcher {
 public:
  NetworkManagerPropertiesWatcher(
      const NetworkPropertiesWatcherCallback& callback) {
    DBusThreadManager::Get()->GetFlimflamManagerClient()->
        SetPropertyChangedHandler(base::Bind(callback,
                                             flimflam::kFlimflamServicePath));
  }
  virtual ~NetworkManagerPropertiesWatcher() {
    DBusThreadManager::Get()->GetFlimflamManagerClient()->
        ResetPropertyChangedHandler();
  }
};

// Class to watch network service's properties with Libcros.
class CrosNetworkServicePropertiesWatcher : public CrosNetworkWatcher {
 public:
  CrosNetworkServicePropertiesWatcher(
      const NetworkPropertiesWatcherCallback& callback,
      const std::string& service_path)
      : callback_(callback),
        monitor_(chromeos::MonitorNetworkServiceProperties(
            &OnNetworkPropertiesChanged, service_path.c_str(), &callback_)) {}
  virtual ~CrosNetworkServicePropertiesWatcher() {
    chromeos::DisconnectNetworkPropertiesMonitor(monitor_);
  }

 private:
  NetworkPropertiesWatcherCallback callback_;
  NetworkPropertiesMonitor monitor_;
};

// Class to watch network service's properties without Libcros.
class NetworkServicePropertiesWatcher : public CrosNetworkWatcher {
 public:
  NetworkServicePropertiesWatcher(
      const NetworkPropertiesWatcherCallback& callback,
      const std::string& service_path) : service_path_(service_path) {
    DBusThreadManager::Get()->GetFlimflamServiceClient()->
        SetPropertyChangedHandler(dbus::ObjectPath(service_path),
                                  base::Bind(callback, service_path));
  }
  virtual ~NetworkServicePropertiesWatcher() {
    DBusThreadManager::Get()->GetFlimflamServiceClient()->
        ResetPropertyChangedHandler(dbus::ObjectPath(service_path_));
  }

 private:
  std::string service_path_;
};

// Class to watch network device's properties with Libcros.
class CrosNetworkDevicePropertiesWatcher : public CrosNetworkWatcher {
 public:
  CrosNetworkDevicePropertiesWatcher(
      const NetworkPropertiesWatcherCallback& callback,
      const std::string& device_path)
      : callback_(callback),
        monitor_(chromeos::MonitorNetworkDeviceProperties(
            &OnNetworkPropertiesChanged, device_path.c_str(), &callback_)) {}
  virtual ~CrosNetworkDevicePropertiesWatcher() {
    chromeos::DisconnectNetworkPropertiesMonitor(monitor_);
  }

 private:
  NetworkPropertiesWatcherCallback callback_;
  NetworkPropertiesMonitor monitor_;
};

// Class to watch network device's properties without Libcros.
class NetworkDevicePropertiesWatcher : public CrosNetworkWatcher {
 public:
  NetworkDevicePropertiesWatcher(
      const NetworkPropertiesWatcherCallback& callback,
      const std::string& device_path) : device_path_(device_path) {
    DBusThreadManager::Get()->GetFlimflamDeviceClient()->
        SetPropertyChangedHandler(dbus::ObjectPath(device_path),
                                  base::Bind(callback, device_path));
  }
  virtual ~NetworkDevicePropertiesWatcher() {
    DBusThreadManager::Get()->GetFlimflamDeviceClient()->
        ResetPropertyChangedHandler(dbus::ObjectPath(device_path_));
  }

 private:
  std::string device_path_;
};

// Class to watch data plan update with Libcros.
class CrosDataPlanUpdateWatcher : public CrosNetworkWatcher {
 public:
  explicit CrosDataPlanUpdateWatcher(
      const DataPlanUpdateWatcherCallback& callback)
      : callback_(callback),
        monitor_(chromeos::MonitorCellularDataPlan(&OnDataPlanUpdate, this)) {}
  virtual ~CrosDataPlanUpdateWatcher() {
    chromeos::DisconnectDataPlanUpdateMonitor(monitor_);
  }

 private:
  static void OnDataPlanUpdate(void* object,
                               const char* modem_service_path,
                               const CellularDataPlanList* data_plan_list) {
    CrosDataPlanUpdateWatcher* watcher =
        static_cast<CrosDataPlanUpdateWatcher*>(object);
    if (modem_service_path && data_plan_list) {
      // Copy contents of |data_plan_list| from libcros to |data_plan_vector|.
      CellularDataPlanVector* data_plan_vector = new CellularDataPlanVector;
      for (size_t i = 0; i < data_plan_list->plans_size; ++i) {
        const CellularDataPlanInfo* info =
            data_plan_list->GetCellularDataPlan(i);
        CellularDataPlan* plan = new CellularDataPlan(*info);
        data_plan_vector->push_back(plan);
        VLOG(2) << " Plan: " << plan->GetPlanDesciption()
                << " : " << plan->GetDataRemainingDesciption();
      }
      // |data_plan_vector| will be owned by callback.
      watcher->callback_.Run(modem_service_path, data_plan_vector);
    }
  }

  DataPlanUpdateWatcherCallback callback_;
  DataPlanUpdateMonitor monitor_;
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
      base::DictionaryValue* data_plan = NULL;
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

// Class to watch sms with Libcros.
class CrosSMSWatcher : public CrosNetworkWatcher {
 public:
  CrosSMSWatcher(const std::string& modem_device_path,
                 MonitorSMSCallback callback,
                 void* object)
      : monitor_(chromeos::MonitorSMS(modem_device_path.c_str(),
                                      callback, object)) {}
  virtual ~CrosSMSWatcher() {
    chromeos::DisconnectSMSMonitor(monitor_);
  }

 private:
  SMSMonitor monitor_;
};

// Does nothing. Used as a callback.
void DoNothing(DBusMethodCallStatus call_status) {}

// Callback used by OnRequestNetworkProperties.
typedef base::Callback<void(const std::string& path,
                            const base::DictionaryValue* properties)
                       > OnRequestNetworkPropertiesCallback;

// Handles responses for RequestNetwork*Properties functions.
void OnRequestNetworkProperties(void* object,
                                const char* path,
                                GHashTable* properties) {
  OnRequestNetworkPropertiesCallback* callback =
      static_cast<OnRequestNetworkPropertiesCallback*>(object);
  DictionaryValue* properties_dictionary = NULL;
  if (properties)
    properties_dictionary =
        ConvertStringValueGHashTableToDictionaryValue(properties);

  // Deleters.
  scoped_ptr<OnRequestNetworkPropertiesCallback> callback_deleter(callback);
  scoped_ptr<DictionaryValue> properties_dictionary_deleter(
      properties_dictionary);

  callback->Run(path, properties_dictionary);
}

// Used as a callback for chromeos::MonitorNetwork*Properties.
void OnNetworkPropertiesChanged(void* object,
                                const char* path,
                                const char* key,
                                const GValue* gvalue) {
  if (key == NULL || gvalue == NULL || path == NULL || object == NULL)
    return;
  NetworkPropertiesWatcherCallback* callback =
      static_cast<NetworkPropertiesWatcherCallback*>(object);
  scoped_ptr<Value> value(ConvertGValueToValue(gvalue));
  callback->Run(path, key, *value);
}

// A callback used to implement CrosRequest*Properties functions.
void RunCallbackWithDictionaryValue(const NetworkPropertiesCallback& callback,
                                    const std::string& path,
                                    DBusMethodCallStatus call_status,
                                    const base::DictionaryValue& value) {
  callback.Run(path, call_status == DBUS_METHOD_CALL_SUCCESS ? &value : NULL);
}

// Used as a callback for FlimflamManagerClient::GetService
void OnGetService(const NetworkPropertiesCallback& callback,
                  DBusMethodCallStatus call_status,
                  const dbus::ObjectPath& service_path) {
  if (call_status == DBUS_METHOD_CALL_SUCCESS) {
    DBusThreadManager::Get()->GetFlimflamServiceClient()->GetProperties(
        service_path, base::Bind(&RunCallbackWithDictionaryValue,
                                 callback,
                                 service_path.value()));
  }
}

// Used as a callback for chromeos::ConfigureService.
void OnConfigureService(void* object,
                        const char* service_path,
                        NetworkMethodErrorType error,
                        const char* error_message) {
  if (error != NETWORK_METHOD_ERROR_NONE) {
    LOG(WARNING) << "Error from ConfigureService callback: "
                 << " Error: " << error << " Message: " << error_message;
  }
}

// Handles responses for network actions.
void OnNetworkAction(void* object,
                     const char* path,
                     NetworkMethodErrorType error,
                     const char* error_message) {
  NetworkOperationCallback* callback =
      static_cast<NetworkOperationCallback*>(object);
  scoped_ptr<NetworkOperationCallback> callback_deleter(callback);

  callback->Run(path, error, error_message ? error_message : "");
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

// Safe string constructor since we can't rely on non NULL pointers
// for string values from libcros.
std::string SafeString(const char* s) {
  return s ? std::string(s) : std::string();
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

// Converts a prefix length to a netmask. (for ipv4)
// e.g. a netmask of 255.255.255.0 has a prefixlen of 24
std::string PrefixlenToNetmask(int32 prefixlen) {
  std::string netmask;
  for (int i = 0; i < 4; i++) {
    int len = 8;
    if (prefixlen >= 8) {
      prefixlen -= 8;
    } else {
      len = prefixlen;
      prefixlen = 0;
    }
    if (i > 0)
      netmask += ".";
    int num = len == 0 ? 0 : ((2L << (len - 1)) - 1) << (8 - len);
    netmask += StringPrintf("%d", num);
  }
  return netmask;
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
  FlimflamIPConfigClient* ipconfig_client =
      DBusThreadManager::Get()->GetFlimflamIPConfigClient();
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
      NetworkIPConfig(device_path, ParseIPConfigType(type_string), address,
                      PrefixlenToNetmask(prefix_len), gateway,
                      name_servers_string));
  return true;
}

// A bool to remember whether we are using Libcros network functions or not.
bool g_libcros_network_functions_enabled = false;

}  // namespace

void SetLibcrosNetworkFunctionsEnabled(bool enabled) {
  g_libcros_network_functions_enabled = enabled;
}

bool CrosActivateCellularModem(const std::string& service_path,
                               const std::string& carrier) {
  if (g_libcros_network_functions_enabled) {
    return chromeos::ActivateCellularModem(service_path.c_str(),
                                           carrier.c_str());
  } else {
    return DBusThreadManager::Get()->GetFlimflamServiceClient()->
        CallActivateCellularModemAndBlock(dbus::ObjectPath(service_path),
                                          carrier);
  }
}

void CrosSetNetworkServiceProperty(const std::string& service_path,
                                   const std::string& property,
                                   const base::Value& value) {
  if (g_libcros_network_functions_enabled) {
    ScopedGValue gvalue(ConvertValueToGValue(value));
    chromeos::SetNetworkServicePropertyGValue(service_path.c_str(),
                                              property.c_str(),
                                              gvalue.get());
  } else {
    DBusThreadManager::Get()->GetFlimflamServiceClient()->SetProperty(
        dbus::ObjectPath(service_path), property, value,
        base::Bind(&DoNothing));
  }
}

void CrosClearNetworkServiceProperty(const std::string& service_path,
                                     const std::string& property) {
  if (g_libcros_network_functions_enabled) {
    chromeos::ClearNetworkServiceProperty(service_path.c_str(),
                                          property.c_str());
  } else {
    DBusThreadManager::Get()->GetFlimflamServiceClient()->ClearProperty(
        dbus::ObjectPath(service_path), property, base::Bind(&DoNothing));
  }
}

void CrosSetNetworkDeviceProperty(const std::string& device_path,
                                  const std::string& property,
                                  const base::Value& value) {
  if (g_libcros_network_functions_enabled) {
    ScopedGValue gvalue(ConvertValueToGValue(value));
    chromeos::SetNetworkDevicePropertyGValue(device_path.c_str(),
                                             property.c_str(),
                                             gvalue.get());
  } else {
    DBusThreadManager::Get()->GetFlimflamDeviceClient()->SetProperty(
        dbus::ObjectPath(device_path), property, value, base::Bind(&DoNothing));
  }
}

void CrosSetNetworkIPConfigProperty(const std::string& ipconfig_path,
                                    const std::string& property,
                                    const base::Value& value) {
  if (g_libcros_network_functions_enabled) {
    ScopedGValue gvalue(ConvertValueToGValue(value));
    chromeos::SetNetworkIPConfigPropertyGValue(ipconfig_path.c_str(),
                                               property.c_str(),
                                               gvalue.get());
  } else {
    DBusThreadManager::Get()->GetFlimflamIPConfigClient()->SetProperty(
        dbus::ObjectPath(ipconfig_path), property, value,
        base::Bind(&DoNothing));
  }
}

void CrosSetNetworkManagerProperty(const std::string& property,
                                   const base::Value& value) {
  if (g_libcros_network_functions_enabled) {
    ScopedGValue gvalue(ConvertValueToGValue(value));
    chromeos::SetNetworkManagerPropertyGValue(property.c_str(), gvalue.get());
  } else {
    DBusThreadManager::Get()->GetFlimflamManagerClient()->SetProperty(
        property, value, base::Bind(&DoNothing));
  }
}

void CrosDeleteServiceFromProfile(const std::string& profile_path,
                                  const std::string& service_path) {
  if (g_libcros_network_functions_enabled) {
    chromeos::DeleteServiceFromProfile(profile_path.c_str(),
                                       service_path.c_str());
  } else {
    DBusThreadManager::Get()->GetFlimflamProfileClient()->DeleteEntry(
        dbus::ObjectPath(profile_path), service_path, base::Bind(&DoNothing));
  }
}

void CrosRequestCellularDataPlanUpdate(const std::string& modem_service_path) {
  if (g_libcros_network_functions_enabled) {
    chromeos::RequestCellularDataPlanUpdate(modem_service_path.c_str());
  } else {
    DBusThreadManager::Get()->GetCashewClient()->RequestDataPlansUpdate(
        modem_service_path);
  }
}

CrosNetworkWatcher* CrosMonitorNetworkManagerProperties(
    const NetworkPropertiesWatcherCallback& callback) {
  if (g_libcros_network_functions_enabled)
    return new CrosNetworkManagerPropertiesWatcher(callback);
  else
    return new NetworkManagerPropertiesWatcher(callback);
}

CrosNetworkWatcher* CrosMonitorNetworkServiceProperties(
    const NetworkPropertiesWatcherCallback& callback,
    const std::string& service_path) {
  if (g_libcros_network_functions_enabled)
    return new CrosNetworkServicePropertiesWatcher(callback, service_path);
  else
    return new NetworkServicePropertiesWatcher(callback, service_path);
}

CrosNetworkWatcher* CrosMonitorNetworkDeviceProperties(
    const NetworkPropertiesWatcherCallback& callback,
    const std::string& device_path) {
  if (g_libcros_network_functions_enabled)
    return new CrosNetworkDevicePropertiesWatcher(callback, device_path);
  else
    return new NetworkDevicePropertiesWatcher(callback, device_path);
}

CrosNetworkWatcher* CrosMonitorCellularDataPlan(
    const DataPlanUpdateWatcherCallback& callback) {
  if (g_libcros_network_functions_enabled)
    return new CrosDataPlanUpdateWatcher(callback);
  else
    return new DataPlanUpdateWatcher(callback);
}

CrosNetworkWatcher* CrosMonitorSMS(const std::string& modem_device_path,
                                   MonitorSMSCallback callback,
                                   void* object) {
  if (g_libcros_network_functions_enabled)
    return new CrosSMSWatcher(modem_device_path, callback, object);
  else
    return new SMSWatcher(modem_device_path, callback, object);
}

void CrosRequestNetworkServiceConnect(
    const std::string& service_path,
    const NetworkOperationCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in OnNetworkAction.
    chromeos::RequestNetworkServiceConnect(
        service_path.c_str(), &OnNetworkAction,
        new NetworkOperationCallback(callback));
  } else {
    DBusThreadManager::Get()->GetFlimflamServiceClient()->Connect(
        dbus::ObjectPath(service_path),
        base::Bind(callback, service_path, NETWORK_METHOD_ERROR_NONE,
                   std::string()),
        base::Bind(&OnNetworkActionError, callback, service_path));
  }
}

void CrosRequestNetworkManagerProperties(
    const NetworkPropertiesCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in
    // OnRequestNetworkProperties.
    chromeos::RequestNetworkManagerProperties(
        &OnRequestNetworkProperties,
        new OnRequestNetworkPropertiesCallback(callback));
  } else {
    DBusThreadManager::Get()->GetFlimflamManagerClient()->GetProperties(
        base::Bind(&RunCallbackWithDictionaryValue,
                   callback,
                   flimflam::kFlimflamServicePath));
  }
}

void CrosRequestNetworkServiceProperties(
    const std::string& service_path,
    const NetworkPropertiesCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in
    // OnRequestNetworkProperties.
    chromeos::RequestNetworkServiceProperties(
        service_path.c_str(),
        &OnRequestNetworkProperties,
        new OnRequestNetworkPropertiesCallback(callback));
  } else {
    DBusThreadManager::Get()->GetFlimflamServiceClient()->GetProperties(
        dbus::ObjectPath(service_path),
        base::Bind(&RunCallbackWithDictionaryValue, callback, service_path));
  }
}

void CrosRequestNetworkDeviceProperties(
    const std::string& device_path,
    const NetworkPropertiesCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in
    // OnRequestNetworkProperties.
    chromeos::RequestNetworkDeviceProperties(
        device_path.c_str(),
        &OnRequestNetworkProperties,
        new OnRequestNetworkPropertiesCallback(callback));
  } else {
    DBusThreadManager::Get()->GetFlimflamDeviceClient()->GetProperties(
        dbus::ObjectPath(device_path),
        base::Bind(&RunCallbackWithDictionaryValue, callback, device_path));
  }
}

void CrosRequestNetworkProfileProperties(
    const std::string& profile_path,
    const NetworkPropertiesCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in
    // OnRequestNetworkProperties.
    chromeos::RequestNetworkProfileProperties(
        profile_path.c_str(),
        &OnRequestNetworkProperties,
        new OnRequestNetworkPropertiesCallback(callback));
  } else {
    DBusThreadManager::Get()->GetFlimflamProfileClient()->GetProperties(
        dbus::ObjectPath(profile_path),
        base::Bind(&RunCallbackWithDictionaryValue, callback, profile_path));
  }
}

void CrosRequestNetworkProfileEntryProperties(
    const std::string& profile_path,
    const std::string& profile_entry_path,
    const NetworkPropertiesCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in
    // OnRequestNetworkProperties.
    chromeos::RequestNetworkProfileEntryProperties(
        profile_path.c_str(),
        profile_entry_path.c_str(),
        &OnRequestNetworkProperties,
        new OnRequestNetworkPropertiesCallback(callback));
  } else {
    DBusThreadManager::Get()->GetFlimflamProfileClient()->GetEntry(
        dbus::ObjectPath(profile_path),
        profile_entry_path,
        base::Bind(&RunCallbackWithDictionaryValue,
                   callback,
                   profile_entry_path));
  }
}

void CrosRequestHiddenWifiNetworkProperties(
    const std::string& ssid,
    const std::string& security,
    const NetworkPropertiesCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in
    // OnRequestNetworkProperties.
    chromeos::RequestHiddenWifiNetworkProperties(
        ssid.c_str(),
        security.c_str(),
        &OnRequestNetworkProperties,
        new OnRequestNetworkPropertiesCallback(callback));
  } else {
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
    // flimflam.Manger.GetService() will apply the property changes in
    // |properties| and return a new or existing service to OnGetService().
    // OnGetService will then call GetProperties which will then call callback.
    DBusThreadManager::Get()->GetFlimflamManagerClient()->GetService(
        properties, base::Bind(&OnGetService, callback));
  }
}

void CrosRequestVirtualNetworkProperties(
    const std::string& service_name,
    const std::string& server_hostname,
    const std::string& provider_type,
    const NetworkPropertiesCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in
    // OnRequestNetworkProperties.
    chromeos::RequestVirtualNetworkProperties(
        service_name.c_str(),
        server_hostname.c_str(),
        provider_type.c_str(),
        &OnRequestNetworkProperties,
        new OnRequestNetworkPropertiesCallback(callback));
  } else {
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

    // flimflam.Manger.GetService() will apply the property changes in
    // |properties| and pass a new or existing service to OnGetService().
    // OnGetService will then call GetProperties which will then call callback.
    DBusThreadManager::Get()->GetFlimflamManagerClient()->GetService(
        properties, base::Bind(&OnGetService, callback));
  }
}

void CrosRequestNetworkServiceDisconnect(const std::string& service_path) {
  if (g_libcros_network_functions_enabled) {
    chromeos::RequestNetworkServiceDisconnect(service_path.c_str());
  } else {
    DBusThreadManager::Get()->GetFlimflamServiceClient()->Disconnect(
        dbus::ObjectPath(service_path), base::Bind(&DoNothing));
  }
}

void CrosRequestRemoveNetworkService(const std::string& service_path) {
  if (g_libcros_network_functions_enabled) {
    chromeos::RequestRemoveNetworkService(service_path.c_str());
  } else {
    DBusThreadManager::Get()->GetFlimflamServiceClient()->Remove(
        dbus::ObjectPath(service_path), base::Bind(&DoNothing));
  }
}

void CrosRequestNetworkScan(const std::string& network_type) {
  if (g_libcros_network_functions_enabled) {
    chromeos::RequestNetworkScan(network_type.c_str());
  } else {
    DBusThreadManager::Get()->GetFlimflamManagerClient()->RequestScan(
        network_type, base::Bind(&DoNothing));
  }
}

void CrosRequestNetworkDeviceEnable(const std::string& network_type,
                                    bool enable) {
  if (g_libcros_network_functions_enabled) {
    chromeos::RequestNetworkDeviceEnable(network_type.c_str(), enable);
  } else {
    if (enable) {
      DBusThreadManager::Get()->GetFlimflamManagerClient()->EnableTechnology(
          network_type, base::Bind(&DoNothing));
    } else {
      DBusThreadManager::Get()->GetFlimflamManagerClient()->DisableTechnology(
          network_type, base::Bind(&DoNothing));
    }
  }
}

void CrosRequestRequirePin(const std::string& device_path,
                           const std::string& pin,
                           bool enable,
                           const NetworkOperationCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in OnNetworkAction.
    chromeos::RequestRequirePin(device_path.c_str(), pin.c_str(), enable,
                                &OnNetworkAction,
                                new NetworkOperationCallback(callback));
  } else {
    DBusThreadManager::Get()->GetFlimflamDeviceClient()->RequirePin(
        dbus::ObjectPath(device_path), pin, enable,
        base::Bind(callback, device_path, NETWORK_METHOD_ERROR_NONE,
                   std::string()),
        base::Bind(&OnNetworkActionError, callback, device_path));
  }
}

void CrosRequestEnterPin(const std::string& device_path,
                         const std::string& pin,
                         const NetworkOperationCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in OnNetworkAction.
    chromeos::RequestEnterPin(device_path.c_str(), pin.c_str(),
                              &OnNetworkAction,
                              new NetworkOperationCallback(callback));
  } else {
    DBusThreadManager::Get()->GetFlimflamDeviceClient()->EnterPin(
        dbus::ObjectPath(device_path), pin,
        base::Bind(callback, device_path, NETWORK_METHOD_ERROR_NONE,
                   std::string()),
        base::Bind(&OnNetworkActionError, callback, device_path));
  }
}

void CrosRequestUnblockPin(const std::string& device_path,
                           const std::string& unblock_code,
                           const std::string& pin,
                           const NetworkOperationCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in OnNetworkAction.
    chromeos::RequestUnblockPin(device_path.c_str(), unblock_code.c_str(),
                                pin.c_str(), &OnNetworkAction,
                                new NetworkOperationCallback(callback));
  } else {
    DBusThreadManager::Get()->GetFlimflamDeviceClient()->UnblockPin(
        dbus::ObjectPath(device_path), unblock_code, pin,
        base::Bind(callback, device_path, NETWORK_METHOD_ERROR_NONE,
                   std::string()),
        base::Bind(&OnNetworkActionError, callback, device_path));
  }
}

void CrosRequestChangePin(const std::string& device_path,
                          const std::string& old_pin,
                          const std::string& new_pin,
                          const NetworkOperationCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in OnNetworkAction.
    chromeos::RequestChangePin(device_path.c_str(), old_pin.c_str(),
                               new_pin.c_str(), &OnNetworkAction,
                               new NetworkOperationCallback(callback));
  } else {
    DBusThreadManager::Get()->GetFlimflamDeviceClient()->ChangePin(
        dbus::ObjectPath(device_path), old_pin, new_pin,
        base::Bind(callback, device_path, NETWORK_METHOD_ERROR_NONE,
                   std::string()),
        base::Bind(&OnNetworkActionError, callback, device_path));
  }
}

void CrosProposeScan(const std::string& device_path) {
  if (g_libcros_network_functions_enabled) {
    chromeos::ProposeScan(device_path.c_str());
  } else {
    DBusThreadManager::Get()->GetFlimflamDeviceClient()->ProposeScan(
        dbus::ObjectPath(device_path), base::Bind(&DoNothing));
  }
}

void CrosRequestCellularRegister(const std::string& device_path,
                                 const std::string& network_id,
                                 const NetworkOperationCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in OnNetworkAction.
    chromeos::RequestCellularRegister(device_path.c_str(), network_id.c_str(),
                                      &OnNetworkAction,
                                      new NetworkOperationCallback(callback));
  } else {
    DBusThreadManager::Get()->GetFlimflamDeviceClient()->Register(
        dbus::ObjectPath(device_path), network_id,
        base::Bind(callback, device_path, NETWORK_METHOD_ERROR_NONE,
                   std::string()),
        base::Bind(&OnNetworkActionError, callback, device_path));
  }
}

bool CrosSetOfflineMode(bool offline) {
  if (g_libcros_network_functions_enabled) {
    return chromeos::SetOfflineMode(offline);
  } else {
    base::FundamentalValue value(offline);
    DBusThreadManager::Get()->GetFlimflamManagerClient()->SetProperty(
        flimflam::kOfflineModeProperty, value, base::Bind(&DoNothing));
    return true;
  }
}

bool CrosListIPConfigs(const std::string& device_path,
                       NetworkIPConfigVector* ipconfig_vector,
                       std::vector<std::string>* ipconfig_paths,
                       std::string* hardware_address) {
  if (hardware_address)
    hardware_address->clear();
  if (g_libcros_network_functions_enabled) {
    if (device_path.empty())
      return false;
    IPConfigStatus* ipconfig_status =
        chromeos::ListIPConfigs(device_path.c_str());
    if (!ipconfig_status)
      return false;
    for (int i = 0; i < ipconfig_status->size; ++i) {
      const IPConfig& ipconfig = ipconfig_status->ips[i];
      ipconfig_vector->push_back(
          NetworkIPConfig(device_path, ipconfig.type,
                          ipconfig.address, ipconfig.netmask,
                          ipconfig.gateway, ipconfig.name_servers));
      if (ipconfig_paths)
        ipconfig_paths->push_back(ipconfig.path);
    }
    if (hardware_address)
      *hardware_address = ipconfig_status->hardware_address;
    chromeos::FreeIPConfigStatus(ipconfig_status);
    return true;
  } else {
    const dbus::ObjectPath device_object_path(device_path);
    FlimflamDeviceClient* device_client =
        DBusThreadManager::Get()->GetFlimflamDeviceClient();
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
}

bool CrosAddIPConfig(const std::string& device_path, IPConfigType type) {
  if (g_libcros_network_functions_enabled) {
    return chromeos::AddIPConfig(device_path.c_str(), type);
  } else {
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
        DBusThreadManager::Get()->GetFlimflamDeviceClient()->
        CallAddIPConfigAndBlock(dbus::ObjectPath(device_path), type_str);
    if (result.value().empty()) {
      LOG(WARNING) <<"Add IPConfig failed: ";
      return false;
    }
    return true;
  }
}

bool CrosRemoveIPConfig(const std::string& ipconfig_path) {
  if (g_libcros_network_functions_enabled) {
    IPConfig dummy_config = {};
    dummy_config.path = ipconfig_path.c_str();
    return chromeos::RemoveIPConfig(&dummy_config);
  } else {
    return DBusThreadManager::Get()->GetFlimflamIPConfigClient()->
        CallRemoveAndBlock(dbus::ObjectPath(ipconfig_path));
  }
}

bool CrosGetWifiAccessPoints(WifiAccessPointVector* result) {
  if (g_libcros_network_functions_enabled) {
    DeviceNetworkList* network_list = chromeos::GetDeviceNetworkList();
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
    chromeos::FreeDeviceNetworkList(network_list);
    return true;
  } else {
    scoped_ptr<base::DictionaryValue> manager_properties(
        DBusThreadManager::Get()->GetFlimflamManagerClient()->
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
          DBusThreadManager::Get()->GetFlimflamDeviceClient()->
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
            DBusThreadManager::Get()->GetFlimflamNetworkClient()->
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
}

void CrosConfigureService(const base::DictionaryValue& properties) {
  if (g_libcros_network_functions_enabled) {
    ScopedGHashTable ghash(
        ConvertDictionaryValueToStringValueGHashTable(properties));
    chromeos::ConfigureService("", ghash.get(), OnConfigureService, NULL);
  } else {
    DBusThreadManager::Get()->GetFlimflamManagerClient()->ConfigureService(
        properties, base::Bind(&DoNothing));
  }
}

}  // namespace chromeos
