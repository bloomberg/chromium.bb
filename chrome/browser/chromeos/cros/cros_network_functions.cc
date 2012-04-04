// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_network_functions.h"

#include <dbus/dbus-glib.h>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace chromeos {

namespace {

// Converts a Value to a GValue.
GValue* ConvertValueToGValue(const Value* value);

// Converts a bool to a GValue.
GValue* ConvertBoolToGValue(bool b) {
  GValue* gvalue = new GValue();
  g_value_init(gvalue, G_TYPE_BOOLEAN);
  g_value_set_boolean(gvalue, b);
  return gvalue;
}

// Converts an int to a GValue.
GValue* ConvertIntToGValue(int i) {
  // Converting to a 32-bit signed int type in particular, since
  // that's what flimflam expects in its DBus API
  GValue* gvalue = new GValue();
  g_value_init(gvalue, G_TYPE_INT);
  g_value_set_int(gvalue, i);
  return gvalue;
}

// Converts a string to a GValue.
GValue* ConvertStringToGValue(const std::string& s) {
  GValue* gvalue = new GValue();
  g_value_init(gvalue, G_TYPE_STRING);
  g_value_set_string(gvalue, s.c_str());
  return gvalue;
}

// Converts a DictionaryValue to a GValue containing a string-to-string
// GHashTable.
GValue* ConvertDictionaryValueToGValue(const DictionaryValue* dict) {
  GHashTable* ghash =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  for (DictionaryValue::key_iterator it = dict->begin_keys();
       it != dict->end_keys(); ++it) {
    std::string key = *it;
    std::string val;
    if (!dict->GetString(key, &val)) {
      NOTREACHED() << "Invalid type in dictionary, key: " << key;
      continue;
    }
    g_hash_table_insert(ghash,
                        g_strdup(const_cast<char*>(key.c_str())),
                        g_strdup(const_cast<char*>(val.c_str())));
  }
  GValue* gvalue = new GValue();
  g_value_init(gvalue, DBUS_TYPE_G_STRING_STRING_HASHTABLE);
  g_value_take_boxed(gvalue, ghash);
  return gvalue;
}

// Unsets and deletes the GValue.
void UnsetAndDeleteGValue(gpointer ptr) {
  GValue* gvalue = static_cast<GValue*>(ptr);
  g_value_unset(gvalue);
  delete gvalue;
}

// Converts a DictionaryValue to a string-to-value GHashTable.
GHashTable* ConvertDictionaryValueToGValueMap(const DictionaryValue* dict) {
  GHashTable* ghash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                            UnsetAndDeleteGValue);
  for (DictionaryValue::key_iterator it = dict->begin_keys();
       it != dict->end_keys(); ++it) {
    std::string key = *it;
    Value* val = NULL;
    if (dict->GetWithoutPathExpansion(key, &val)) {
      g_hash_table_insert(ghash,
                          g_strdup(const_cast<char*>(key.c_str())),
                          ConvertValueToGValue(val));
    } else {
      VLOG(2) << "Could not insert key " << key << " into hash";
    }
  }
  return ghash;
}

GValue* ConvertValueToGValue(const Value* value) {
  switch (value->GetType()) {
    case Value::TYPE_BOOLEAN: {
      bool out;
      if (value->GetAsBoolean(&out))
        return ConvertBoolToGValue(out);
      break;
    }
    case Value::TYPE_INTEGER: {
      int out;
      if (value->GetAsInteger(&out))
        return ConvertIntToGValue(out);
      break;
    }
    case Value::TYPE_STRING: {
      std::string out;
      if (value->GetAsString(&out))
        return ConvertStringToGValue(out);
      break;
    }
    case Value::TYPE_DICTIONARY: {
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(value);
      return ConvertDictionaryValueToGValue(dict);
    }
    case Value::TYPE_NULL:
    case Value::TYPE_DOUBLE:
    case Value::TYPE_BINARY:
    case Value::TYPE_LIST:
      // Other Value types shouldn't be passed through this mechanism.
      NOTREACHED() << "Unconverted Value of type: " << value->GetType();
      return new GValue();
  }
  NOTREACHED() << "Value conversion failed, type: " << value->GetType();
  return new GValue();
}

}  // namespace

ScopedGValue::ScopedGValue() {}

ScopedGValue::ScopedGValue(GValue* value) : value_(value) {}

ScopedGValue::~ScopedGValue() {
  reset(NULL);
}

void ScopedGValue::reset(GValue* value) {
  if (value_.get())
    g_value_unset(value_.get());
  value_.reset(value);
}

ScopedGHashTable::ScopedGHashTable() : table_(NULL) {}

ScopedGHashTable::ScopedGHashTable(GHashTable* table) : table_(table) {}

ScopedGHashTable::~ScopedGHashTable() {
  reset(NULL);
}

void ScopedGHashTable::reset(GHashTable* table) {
  if (table_)
    g_hash_table_unref(table_);
  table_ = table;
}

bool CrosActivateCellularModem(const char* service_path, const char* carrier) {
  return chromeos::ActivateCellularModem(service_path, carrier);
}

void CrosSetNetworkServiceProperty(const char* service_path,
                                   const char* property,
                                   const base::Value& value) {
  ScopedGValue gvalue(ConvertValueToGValue(&value));
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
  ScopedGValue gvalue(ConvertValueToGValue(&value));
  chromeos::SetNetworkDevicePropertyGValue(device_path, property, gvalue.get());
}

void CrosSetNetworkIPConfigProperty(const char* ipconfig_path,
                                    const char* property,
                                    const base::Value& value) {
  ScopedGValue gvalue(ConvertValueToGValue(&value));
  chromeos::SetNetworkIPConfigPropertyGValue(ipconfig_path, property,
                                             gvalue.get());
}

void CrosSetNetworkManagerProperty(const char* property,
                                   const base::Value& value) {
  ScopedGValue gvalue(ConvertValueToGValue(&value));
  chromeos::SetNetworkManagerPropertyGValue(property, gvalue.get());
}

void CrosDeleteServiceFromProfile(const char* profile_path,
                                  const char* service_path) {
  chromeos::DeleteServiceFromProfile(profile_path, service_path);
}

void CrosRequestCellularDataPlanUpdate(const char* modem_service_path) {
  chromeos::RequestCellularDataPlanUpdate(modem_service_path);
}

NetworkPropertiesMonitor CrosMonitorNetworkManagerProperties(
    MonitorPropertyGValueCallback callback,
    void* object) {
  return chromeos::MonitorNetworkManagerProperties(callback, object);
}

NetworkPropertiesMonitor CrosMonitorNetworkServiceProperties(
    MonitorPropertyGValueCallback callback,
    const char* service_path,
    void* object) {
  return chromeos::MonitorNetworkServiceProperties(
      callback, service_path, object);
}

NetworkPropertiesMonitor CrosMonitorNetworkDeviceProperties(
    MonitorPropertyGValueCallback callback,
    const char* device_path,
    void* object) {
  return chromeos::MonitorNetworkDeviceProperties(
      callback, device_path, object);
}

void CrosDisconnectNetworkPropertiesMonitor(
    NetworkPropertiesMonitor monitor) {
  DisconnectNetworkPropertiesMonitor(monitor);
}

DataPlanUpdateMonitor CrosMonitorCellularDataPlan(
    MonitorDataPlanCallback callback,
    void* object) {
  return chromeos::MonitorCellularDataPlan(callback, object);
}

void CrosDisconnectDataPlanUpdateMonitor(DataPlanUpdateMonitor monitor) {
  chromeos::DisconnectDataPlanUpdateMonitor(monitor);
}

SMSMonitor CrosMonitorSMS(const char* modem_device_path,
                          MonitorSMSCallback callback,
                          void* object) {
  return chromeos::MonitorSMS(modem_device_path, callback, object);
}

void CrosDisconnectSMSMonitor(SMSMonitor monitor) {
  chromeos::DisconnectSMSMonitor(monitor);
}

void CrosRequestNetworkServiceConnect(const char* service_path,
                                      NetworkActionCallback callback,
                                      void* object) {
  chromeos::RequestNetworkServiceConnect(service_path, callback, object);
}

void CrosRequestNetworkManagerProperties(
    NetworkPropertiesGValueCallback callback,
    void* object) {
  chromeos::RequestNetworkManagerProperties(callback, object);
}

void CrosRequestNetworkServiceProperties(
    const char* service_path,
    NetworkPropertiesGValueCallback callback,
    void* object) {
  chromeos::RequestNetworkServiceProperties(service_path, callback, object);
}

void CrosRequestNetworkDeviceProperties(
    const char* device_path,
    NetworkPropertiesGValueCallback callback,
    void* object) {
  chromeos::RequestNetworkDeviceProperties(device_path, callback, object);
}

void CrosRequestNetworkProfileProperties(
    const char* profile_path,
    NetworkPropertiesGValueCallback callback,
    void* object) {
  chromeos::RequestNetworkProfileProperties(profile_path, callback, object);
}

void CrosRequestNetworkProfileEntryProperties(
    const char* profile_path,
    const char* profile_entry_path,
    NetworkPropertiesGValueCallback callback,
    void* object) {
  chromeos::RequestNetworkProfileEntryProperties(
      profile_path, profile_entry_path, callback, object);
}

void CrosRequestHiddenWifiNetworkProperties(
    const char* ssid,
    const char* security,
    NetworkPropertiesGValueCallback callback,
    void* object) {
  chromeos::RequestHiddenWifiNetworkProperties(
      ssid, security, callback, object);
}

void CrosRequestVirtualNetworkProperties(
    const char* service_name,
    const char* server_hostname,
    const char* provider_type,
    NetworkPropertiesGValueCallback callback,
    void* object) {
  chromeos::RequestVirtualNetworkProperties(service_name, server_hostname,
                                            provider_type, callback, object);
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
  ScopedGHashTable ghash(ConvertDictionaryValueToGValueMap(&properties));
  chromeos::ConfigureService(identifier, ghash.get(), callback, object);
}

}  // namespace chromeos
