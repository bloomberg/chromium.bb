// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_network_functions.h"

namespace chromeos {

bool CrosActivateCellularModem(const char* service_path, const char* carrier) {
  return chromeos::ActivateCellularModem(service_path, carrier);
}

void CrosSetNetworkServicePropertyGValue(const char* service_path,
                                         const char* property,
                                         const GValue* gvalue) {
  chromeos::SetNetworkServicePropertyGValue(service_path, property, gvalue);
}

void CrosClearNetworkServiceProperty(const char* service_path,
                                     const char* property) {
  chromeos::ClearNetworkServiceProperty(service_path, property);
}

void CrosSetNetworkDevicePropertyGValue(const char* device_path,
                                        const char* property,
                                        const GValue* gvalue) {
  chromeos::SetNetworkDevicePropertyGValue(device_path, property, gvalue);
}

void CrosSetNetworkIPConfigPropertyGValue(const char* ipconfig_path,
                                          const char* property,
                                          const GValue* gvalue) {
  chromeos::SetNetworkIPConfigPropertyGValue(ipconfig_path, property, gvalue);
}

void CrosSetNetworkManagerPropertyGValue(const char* property,
                                         const GValue* gvalue) {
  chromeos::SetNetworkManagerPropertyGValue(property, gvalue);
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
                          const GHashTable* properties,
                          NetworkActionCallback callback,
                          void* object) {
  chromeos::ConfigureService(identifier, properties, callback, object);
}

}  // namespace chromeos
