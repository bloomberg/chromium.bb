// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_network_functions.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/gvalue_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/flimflam_device_client.h"
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

// Class to watch sms Libcros.
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
typedef base::Callback<void(const char* path,
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
                                    const char* path,
                                    DBusMethodCallStatus call_status,
                                    const base::DictionaryValue& value) {
  callback.Run(path, call_status == DBUS_METHOD_CALL_SUCCESS ? &value : NULL);
}

// A bool to remember whether we are using Libcros network functions or not.
bool g_libcros_network_functions_enabled = true;

}  // namespace

void SetLibcrosNetworkFunctionsEnabled(bool enabled) {
  g_libcros_network_functions_enabled = enabled;
}

bool CrosActivateCellularModem(const char* service_path, const char* carrier) {
  return chromeos::ActivateCellularModem(service_path, carrier);
}

void CrosSetNetworkServiceProperty(const char* service_path,
                                   const char* property,
                                   const base::Value& value) {
  ScopedGValue gvalue(ConvertValueToGValue(value));
  chromeos::SetNetworkServicePropertyGValue(service_path, property,
                                            gvalue.get());
}

void CrosClearNetworkServiceProperty(const char* service_path,
                                     const char* property) {
  chromeos::ClearNetworkServiceProperty(service_path, property);
}

void CrosSetNetworkDeviceProperty(const char* device_path,
                                  const char* property,
                                  const base::Value& value) {
  ScopedGValue gvalue(ConvertValueToGValue(value));
  chromeos::SetNetworkDevicePropertyGValue(device_path, property, gvalue.get());
}

void CrosSetNetworkIPConfigProperty(const char* ipconfig_path,
                                    const char* property,
                                    const base::Value& value) {
  ScopedGValue gvalue(ConvertValueToGValue(value));
  chromeos::SetNetworkIPConfigPropertyGValue(ipconfig_path, property,
                                             gvalue.get());
}

void CrosSetNetworkManagerProperty(const char* property,
                                   const base::Value& value) {
  ScopedGValue gvalue(ConvertValueToGValue(value));
  chromeos::SetNetworkManagerPropertyGValue(property, gvalue.get());
}

void CrosDeleteServiceFromProfile(const char* profile_path,
                                  const char* service_path) {
  if (g_libcros_network_functions_enabled) {
    chromeos::DeleteServiceFromProfile(profile_path, service_path);
  } else {
    DBusThreadManager::Get()->GetFlimflamProfileClient()->DeleteEntry(
        dbus::ObjectPath(profile_path), service_path, base::Bind(&DoNothing));
  }
}

void CrosRequestCellularDataPlanUpdate(const char* modem_service_path) {
  chromeos::RequestCellularDataPlanUpdate(modem_service_path);
}

CrosNetworkWatcher* CrosMonitorNetworkManagerProperties(
    const NetworkPropertiesWatcherCallback& callback) {
  return new CrosNetworkManagerPropertiesWatcher(callback);
}

CrosNetworkWatcher* CrosMonitorNetworkServiceProperties(
    const NetworkPropertiesWatcherCallback& callback,
    const std::string& service_path) {
  return new CrosNetworkServicePropertiesWatcher(callback, service_path);
}

CrosNetworkWatcher* CrosMonitorNetworkDeviceProperties(
    const NetworkPropertiesWatcherCallback& callback,
    const std::string& device_path) {
  return new CrosNetworkDevicePropertiesWatcher(callback, device_path);
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

void CrosRequestNetworkServiceConnect(const char* service_path,
                                      NetworkActionCallback callback,
                                      void* object) {
  chromeos::RequestNetworkServiceConnect(service_path, callback, object);
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
    const char* service_path,
    const NetworkPropertiesCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in
    // OnRequestNetworkProperties.
    chromeos::RequestNetworkServiceProperties(
        service_path,
        &OnRequestNetworkProperties,
        new OnRequestNetworkPropertiesCallback(callback));
  } else {
    DBusThreadManager::Get()->GetFlimflamServiceClient()->GetProperties(
        dbus::ObjectPath(service_path),
        base::Bind(&RunCallbackWithDictionaryValue, callback, service_path));
  }
}

void CrosRequestNetworkDeviceProperties(
    const char* device_path,
    const NetworkPropertiesCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in
    // OnRequestNetworkProperties.
    chromeos::RequestNetworkDeviceProperties(
        device_path,
        &OnRequestNetworkProperties,
        new OnRequestNetworkPropertiesCallback(callback));
  } else {
    DBusThreadManager::Get()->GetFlimflamDeviceClient()->GetProperties(
        dbus::ObjectPath(device_path),
        base::Bind(&RunCallbackWithDictionaryValue, callback, device_path));
  }
}

void CrosRequestNetworkProfileProperties(
    const char* profile_path,
    const NetworkPropertiesCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in
    // OnRequestNetworkProperties.
    chromeos::RequestNetworkProfileProperties(
        profile_path,
        &OnRequestNetworkProperties,
        new OnRequestNetworkPropertiesCallback(callback));
  } else {
    DBusThreadManager::Get()->GetFlimflamProfileClient()->GetProperties(
        dbus::ObjectPath(profile_path),
        base::Bind(&RunCallbackWithDictionaryValue, callback, profile_path));
  }
}

void CrosRequestNetworkProfileEntryProperties(
    const char* profile_path,
    const char* profile_entry_path,
    const NetworkPropertiesCallback& callback) {
  if (g_libcros_network_functions_enabled) {
    // The newly allocated callback will be deleted in
    // OnRequestNetworkProperties.
    chromeos::RequestNetworkProfileEntryProperties(
        profile_path,
        profile_entry_path,
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
    const char* ssid,
    const char* security,
    const NetworkPropertiesCallback& callback) {
  // The newly allocated callback will be deleted in OnRequestNetworkProperties.
  chromeos::RequestHiddenWifiNetworkProperties(
      ssid,
      security,
      &OnRequestNetworkProperties,
      new OnRequestNetworkPropertiesCallback(callback));
}

void CrosRequestVirtualNetworkProperties(
    const char* service_name,
    const char* server_hostname,
    const char* provider_type,
    const NetworkPropertiesCallback& callback) {
  // The newly allocated callback will be deleted in OnRequestNetworkProperties.
  chromeos::RequestVirtualNetworkProperties(
      service_name,
      server_hostname,
      provider_type,
      &OnRequestNetworkProperties,
      new OnRequestNetworkPropertiesCallback(callback));
}

void CrosRequestNetworkServiceDisconnect(const char* service_path) {
  chromeos::RequestNetworkServiceDisconnect(service_path);
}

void CrosRequestRemoveNetworkService(const char* service_path) {
  chromeos::RequestRemoveNetworkService(service_path);
}

void CrosRequestNetworkScan(const char* network_type) {
  chromeos::RequestNetworkScan(network_type);
}

void CrosRequestNetworkDeviceEnable(const char* network_type, bool enable) {
  chromeos::RequestNetworkDeviceEnable(network_type, enable);
}

void CrosRequestRequirePin(const char* device_path,
                           const char* pin,
                           bool enable,
                           NetworkActionCallback callback,
                           void* object) {
  chromeos::RequestRequirePin(device_path, pin, enable, callback, object);
}

void CrosRequestEnterPin(const char* device_path,
                         const char* pin,
                         NetworkActionCallback callback,
                         void* object) {
  chromeos::RequestEnterPin(device_path, pin, callback, object);
}

void CrosRequestUnblockPin(const char* device_path,
                           const char* unblock_code,
                           const char* pin,
                           NetworkActionCallback callback,
                           void* object) {
  chromeos::RequestUnblockPin(device_path, unblock_code, pin, callback, object);
}

void CrosRequestChangePin(const char* device_path,
                          const char* old_pin,
                          const char* new_pin,
                          NetworkActionCallback callback,
                          void* object) {
  chromeos::RequestChangePin(device_path, old_pin, new_pin, callback, object);
}

void CrosProposeScan(const char* device_path) {
  chromeos::ProposeScan(device_path);
}

void CrosRequestCellularRegister(const char* device_path,
                                 const char* network_id,
                                 chromeos::NetworkActionCallback callback,
                                 void* object) {
  chromeos::RequestCellularRegister(device_path, network_id, callback, object);
}

bool CrosSetOfflineMode(bool offline) {
  return chromeos::SetOfflineMode(offline);
}

IPConfigStatus* CrosListIPConfigs(const char* device_path) {
  return chromeos::ListIPConfigs(device_path);
}

bool CrosAddIPConfig(const char* device_path, IPConfigType type) {
  return chromeos::AddIPConfig(device_path, type);
}

bool CrosRemoveIPConfig(IPConfig* config) {
  return chromeos::RemoveIPConfig(config);
}

void CrosFreeIPConfigStatus(IPConfigStatus* status) {
  chromeos::FreeIPConfigStatus(status);
}

DeviceNetworkList* CrosGetDeviceNetworkList() {
  return chromeos::GetDeviceNetworkList();
}

void CrosFreeDeviceNetworkList(DeviceNetworkList* network_list) {
  chromeos::FreeDeviceNetworkList(network_list);
}

void CrosConfigureService(const char* identifier,
                          const base::DictionaryValue& properties,
                          NetworkActionCallback callback,
                          void* object) {
  ScopedGHashTable ghash(
      ConvertDictionaryValueToStringValueGHashTable(properties));
  chromeos::ConfigureService(identifier, ghash.get(), callback, object);
}

}  // namespace chromeos
