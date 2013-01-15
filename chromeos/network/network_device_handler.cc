// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_device_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {
const char kLogModule[] = "NetworkDeviceHandler";
}

namespace chromeos {

static NetworkDeviceHandler* g_network_device_handler = NULL;

//------------------------------------------------------------------------------
// NetworkDeviceHandler public methods

NetworkDeviceHandler::~NetworkDeviceHandler() {
  DBusThreadManager::Get()->GetShillManagerClient()->
      RemovePropertyChangedObserver(this);
}

void NetworkDeviceHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NetworkDeviceHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
void NetworkDeviceHandler::Initialize() {
  CHECK(!g_network_device_handler);
  g_network_device_handler = new NetworkDeviceHandler();
  g_network_device_handler->Init();
}

// static
void NetworkDeviceHandler::Shutdown() {
  CHECK(g_network_device_handler);
  delete g_network_device_handler;
  g_network_device_handler = NULL;
}

// static
NetworkDeviceHandler* NetworkDeviceHandler::Get() {
  CHECK(g_network_device_handler)
      << "NetworkDeviceHandler::Get() called before Initialize()";
  return g_network_device_handler;
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

NetworkDeviceHandler::NetworkDeviceHandler()
    : devices_ready_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

void NetworkDeviceHandler::Init() {
  ShillManagerClient* shill_manager =
      DBusThreadManager::Get()->GetShillManagerClient();
  shill_manager->GetProperties(
      base::Bind(&NetworkDeviceHandler::ManagerPropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr()));
  shill_manager->AddPropertyChangedObserver(this);
}

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
  } else {
    GetDeviceProperties(device_path, properties);
  }
  pending_device_paths_.erase(device_path);
  if (pending_device_paths_.empty()) {
    devices_ready_ = true;
    FOR_EACH_OBSERVER(Observer, observers_, NetworkDevicesUpdated(devices_));
  }
}

void NetworkDeviceHandler::GetDeviceProperties(
    const std::string& device_path,
    const base::DictionaryValue& properties) {
  std::string type;
  if (!properties.GetStringWithoutPathExpansion(
          flimflam::kTypeProperty, &type)) {
    LOG(WARNING) << "Failed to parse Type property for " << device_path;
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
}

//------------------------------------------------------------------------------

NetworkDeviceHandler::Device::Device()
    : powered(false), scanning(false), scan_interval(0) {
}

NetworkDeviceHandler::Device::~Device() {
}

}  // namespace chromeos
