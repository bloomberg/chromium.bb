// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_device_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_network_client.h"
#include "chromeos/network/network_event_log.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

//------------------------------------------------------------------------------
// NetworkDeviceHandler public methods

NetworkDeviceHandler::NetworkDeviceHandler()
    : devices_ready_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

NetworkDeviceHandler::~NetworkDeviceHandler() {
  DBusThreadManager::Get()->GetShillManagerClient()->
      RemovePropertyChangedObserver(this);
}

void NetworkDeviceHandler::Init() {
  ShillManagerClient* shill_manager =
      DBusThreadManager::Get()->GetShillManagerClient();
  shill_manager->GetProperties(
      base::Bind(&NetworkDeviceHandler::ManagerPropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr()));
  shill_manager->AddPropertyChangedObserver(this);
}

void NetworkDeviceHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NetworkDeviceHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

//------------------------------------------------------------------------------
// ShillPropertyChangedObserver overrides

void NetworkDeviceHandler::OnPropertyChanged(const std::string& key,
                                             const base::Value& value) {
  if (key != flimflam::kDevicesProperty)
    return;
  const base::ListValue* devices = NULL;
  if (!value.GetAsList(&devices)) {
    LOG(ERROR) << "Failed to parse Devices property.";
    return;
  }
  DevicePropertyChanged(devices);
}

//------------------------------------------------------------------------------
// Private methods

void NetworkDeviceHandler::ManagerPropertiesCallback(
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "Failed to get Manager properties: " << call_status;
    return;
  }
  const base::ListValue* devices = NULL;
  if (!properties.GetListWithoutPathExpansion(
          flimflam::kDevicesProperty, &devices)) {
    LOG(WARNING) << "Devices property not found";
    return;
  }
  DevicePropertyChanged(devices);
}

void NetworkDeviceHandler::DevicePropertyChanged(
    const base::ListValue* devices) {
  DCHECK(devices);
  devices_ready_ = false;
  devices_.clear();
  pending_device_paths_.clear();
  for (size_t i = 0; i < devices->GetSize(); i++) {
    std::string device_path;
    if (!devices->GetString(i, &device_path)) {
      LOG(WARNING) << "Failed to parse device[" << i << "]";
      continue;
    }
    pending_device_paths_.insert(device_path);
    DBusThreadManager::Get()->GetShillDeviceClient()->GetProperties(
        dbus::ObjectPath(device_path),
        base::Bind(&NetworkDeviceHandler::DevicePropertiesCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   device_path));
  }
}

void NetworkDeviceHandler::DevicePropertiesCallback(
    const std::string& device_path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "Failed to get Device properties for " << device_path
               << ": " << call_status;
    DeviceReady(device_path);
    return;
  }
  std::string type;
  if (!properties.GetStringWithoutPathExpansion(
          flimflam::kTypeProperty, &type)) {
    LOG(WARNING) << "Failed to parse Type property for " << device_path;
    DeviceReady(device_path);
    return;
  }
  Device& device = devices_[device_path];
  device.type = type;
  properties.GetBooleanWithoutPathExpansion(
      flimflam::kPoweredProperty, &device.powered);
  properties.GetBooleanWithoutPathExpansion(
      flimflam::kScanningProperty, &device.scanning);
  properties.GetIntegerWithoutPathExpansion(
      flimflam::kScanIntervalProperty, &device.scan_interval);

  if (!device.powered ||
      (type != flimflam::kTypeWifi && type != flimflam::kTypeWimax)) {
    DeviceReady(device_path);
    return;
  }

  // Get WifiAccessPoint data for each Network property (only for powered
  // wifi or wimax devices).
  const base::ListValue* networks = NULL;
  if (!properties.GetListWithoutPathExpansion(
          flimflam::kNetworksProperty, &networks)) {
    LOG(WARNING) << "Failed to parse Networks property for " << device_path;
    DeviceReady(device_path);
    return;
  }

  for (size_t i = 0; i < networks->GetSize(); ++i) {
    std::string network_path;
    if (!networks->GetString(i, &network_path)) {
      LOG(WARNING) << "Failed tp parse Networks[" << i << "]"
                   << " for device: " << device_path;
      continue;
    }
    pending_network_paths_[device_path].insert(network_path);
    DBusThreadManager::Get()->GetShillNetworkClient()->GetProperties(
        dbus::ObjectPath(device_path),
        base::Bind(&NetworkDeviceHandler::NetworkPropertiesCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   device_path,
                   network_path));
  }
}

void NetworkDeviceHandler::NetworkPropertiesCallback(
    const std::string& device_path,
    const std::string& network_path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "Failed to get Network properties for " << network_path
               << ", for device: " << device_path << ": " << call_status;
    DeviceNetworkReady(device_path, network_path);
    return;
  }

  // Using the scan interval as a proxy for approximate age.
  // TODO(joth): Replace with actual age, when available from dbus.
  Device& device = devices_[device_path];
  WifiAccessPoint& wap = device.wifi_access_points[network_path];
  properties.GetStringWithoutPathExpansion(
      flimflam::kAddressProperty, &wap.mac_address);
  properties.GetStringWithoutPathExpansion(
      flimflam::kNameProperty, &wap.name);
  int age_seconds = device.scan_interval;
  wap.timestamp = base::Time::Now() - base::TimeDelta::FromSeconds(age_seconds);
  properties.GetIntegerWithoutPathExpansion(
      flimflam::kSignalStrengthProperty, &wap.signal_strength);
  properties.GetIntegerWithoutPathExpansion(
      flimflam::kWifiChannelProperty, &wap.channel);
  DeviceNetworkReady(device_path, network_path);
}

void NetworkDeviceHandler::DeviceNetworkReady(const std::string& device_path,
                                              const std::string& network_path) {
  pending_network_paths_[device_path].erase(network_path);
  if (pending_network_paths_[device_path].empty())
    DeviceReady(device_path);
}

void NetworkDeviceHandler::DeviceReady(const std::string& device_path) {
  pending_device_paths_.erase(device_path);
  if (pending_device_paths_.empty()) {
    devices_ready_ = true;
    FOR_EACH_OBSERVER(Observer, observers_, NetworkDevicesUpdated(devices_));
  }
}

//------------------------------------------------------------------------------

NetworkDeviceHandler::Device::Device()
    : powered(false), scanning(false), scan_interval(0) {
}

NetworkDeviceHandler::Device::~Device() {
}

}  // namespace chromeos
