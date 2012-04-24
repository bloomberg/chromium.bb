// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_network_functions.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/gvalue_util.h"
#include "chromeos/dbus/cashew_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/flimflam_device_client.h"
#include "chromeos/dbus/flimflam_ipconfig_client.h"
#include "chromeos/dbus/flimflam_manager_client.h"
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
  CrosDataPlanUpdateWatcher(MonitorDataPlanCallback callback, void* object)
      : monitor_(chromeos::MonitorCellularDataPlan(callback, object)) {}
  virtual ~CrosDataPlanUpdateWatcher() {
    chromeos::DisconnectDataPlanUpdateMonitor(monitor_);
  }

 private:
  DataPlanUpdateMonitor monitor_;
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

// Safe string constructor since we can't rely on non NULL pointers
// for string values from libcros.
std::string SafeString(const char* s) {
  return s ? std::string(s) : std::string();
}

// A bool to remember whether we are using Libcros network functions or not.
bool g_libcros_network_functions_enabled = true;

}  // namespace

void SetLibcrosNetworkFunctionsEnabled(bool enabled) {
  g_libcros_network_functions_enabled = enabled;
}

bool CrosActivateCellularModem(const std::string& service_path,
                               const std::string& carrier) {
  return chromeos::ActivateCellularModem(service_path.c_str(), carrier.c_str());
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
    MonitorDataPlanCallback callback, void* object) {
  return new CrosDataPlanUpdateWatcher(callback, object);
}

CrosNetworkWatcher* CrosMonitorSMS(const std::string& modem_device_path,
                                   MonitorSMSCallback callback,
                                   void* object) {
  return new CrosSMSWatcher(modem_device_path, callback, object);
}

void CrosRequestNetworkServiceConnect(const std::string& service_path,
                                      NetworkActionCallback callback,
                                      void* object) {
  chromeos::RequestNetworkServiceConnect(service_path.c_str(), callback,
                                         object);
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
                           NetworkActionCallback callback,
                           void* object) {
  chromeos::RequestRequirePin(device_path.c_str(), pin.c_str(), enable,
                              callback, object);
}

void CrosRequestEnterPin(const std::string& device_path,
                         const std::string& pin,
                         NetworkActionCallback callback,
                         void* object) {
  chromeos::RequestEnterPin(device_path.c_str(), pin.c_str(), callback, object);
}

void CrosRequestUnblockPin(const std::string& device_path,
                           const std::string& unblock_code,
                           const std::string& pin,
                           NetworkActionCallback callback,
                           void* object) {
  chromeos::RequestUnblockPin(device_path.c_str(), unblock_code.c_str(),
                              pin.c_str(), callback, object);
}

void CrosRequestChangePin(const std::string& device_path,
                          const std::string& old_pin,
                          const std::string& new_pin,
                          NetworkActionCallback callback,
                          void* object) {
  chromeos::RequestChangePin(device_path.c_str(), old_pin.c_str(),
                             new_pin.c_str(), callback, object);
}

void CrosProposeScan(const std::string& device_path) {
  chromeos::ProposeScan(device_path.c_str());
}

void CrosRequestCellularRegister(const std::string& device_path,
                                 const std::string& network_id,
                                 chromeos::NetworkActionCallback callback,
                                 void* object) {
  chromeos::RequestCellularRegister(device_path.c_str(), network_id.c_str(),
                                    callback, object);
}

bool CrosSetOfflineMode(bool offline) {
  return chromeos::SetOfflineMode(offline);
}

IPConfigStatus* CrosListIPConfigs(const std::string& device_path) {
  return chromeos::ListIPConfigs(device_path.c_str());
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

bool CrosRemoveIPConfig(IPConfig* config) {
  if (g_libcros_network_functions_enabled) {
    return chromeos::RemoveIPConfig(config);
  } else {
    return DBusThreadManager::Get()->GetFlimflamIPConfigClient()->
        CallRemoveAndBlock(dbus::ObjectPath(config->path));
  }
}

void CrosFreeIPConfigStatus(IPConfigStatus* status) {
  chromeos::FreeIPConfigStatus(status);
}

bool CrosGetWifiAccessPoints(WifiAccessPointVector* result) {
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
