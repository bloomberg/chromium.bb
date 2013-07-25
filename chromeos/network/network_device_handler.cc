// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_device_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_event_log.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kDevicePath[] = "devicePath";

// TODO(armansito): Add bindings for these to service_constants.h
// (crbug.com/256889)
const char kShillErrorFailure[] = "org.chromium.flimflam.Error.Failure";
const char kShillErrorNotSupported[] =
    "org.chromium.flimflam.Error.NotSupported";

std::string GetErrorNameForShillError(const std::string& shill_error_name) {
  // TODO(armansito): Use the new SIM error names once the ones below get
  // deprecated (crbug.com/256855)
  if (shill_error_name == kShillErrorFailure)
    return NetworkDeviceHandler::kErrorFailure;
  if (shill_error_name == kShillErrorNotSupported)
    return NetworkDeviceHandler::kErrorNotSupported;
  if (shill_error_name == flimflam::kErrorIncorrectPinMsg)
    return NetworkDeviceHandler::kErrorIncorrectPin;
  if (shill_error_name == flimflam::kErrorPinBlockedMsg)
    return NetworkDeviceHandler::kErrorPinBlocked;
  if (shill_error_name == flimflam::kErrorPinRequiredMsg)
    return NetworkDeviceHandler::kErrorPinRequired;
  return NetworkDeviceHandler::kErrorUnknown;
}

void HandleShillCallFailure(
    const std::string& device_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& shill_error_name,
    const std::string& shill_error_message) {
  network_handler::ShillErrorCallbackFunction(
      GetErrorNameForShillError(shill_error_name),
      device_path,
      error_callback,
      shill_error_name,
      shill_error_message);
}

void IPConfigRefreshCallback(const std::string& ipconfig_path,
                             DBusMethodCallStatus call_status) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    NET_LOG_ERROR(
        base::StringPrintf("IPConfigs.Refresh Failed: %d", call_status),
        ipconfig_path);
  } else {
    NET_LOG_EVENT("IPConfigs.Refresh Succeeded", ipconfig_path);
  }
}

void RefreshIPConfigsCallback(
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback,
    const std::string& device_path,
    const base::DictionaryValue& properties) {
  const ListValue* ip_configs;
  if (!properties.GetListWithoutPathExpansion(
          flimflam::kIPConfigsProperty, &ip_configs)) {
    NET_LOG_ERROR("RequestRefreshIPConfigs Failed", device_path);
    network_handler::ShillErrorCallbackFunction(
        "RequestRefreshIPConfigs Failed",
        device_path,
        error_callback,
        std::string("Missing ") + flimflam::kIPConfigsProperty, "");
    return;
  }

  for (size_t i = 0; i < ip_configs->GetSize(); i++) {
    std::string ipconfig_path;
    if (!ip_configs->GetString(i, &ipconfig_path))
      continue;
    DBusThreadManager::Get()->GetShillIPConfigClient()->Refresh(
        dbus::ObjectPath(ipconfig_path),
        base::Bind(&IPConfigRefreshCallback, ipconfig_path));
  }
  // It is safe to invoke |callback| here instead of waiting for the
  // IPConfig.Refresh callbacks to complete because the Refresh DBus calls will
  // be executed in order and thus before any further DBus requests that
  // |callback| may issue.
  if (!callback.is_null())
    callback.Run();
}

void ProposeScanCallback(
    const std::string& device_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback,
    DBusMethodCallStatus call_status) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    NET_LOG_ERROR(
        base::StringPrintf("Device.ProposeScan failed: %d", call_status),
        device_path);
    network_handler::ShillErrorCallbackFunction(
        "Device.ProposeScan Failed",
        device_path,
        error_callback,
        base::StringPrintf("DBus call failed: %d", call_status), "");
    return;
  }
  NET_LOG_EVENT("Device.ProposeScan succeeded.", device_path);
  if (!callback.is_null())
    callback.Run();
}

}  // namespace

const char NetworkDeviceHandler::kErrorFailure[] = "failure";
const char NetworkDeviceHandler::kErrorIncorrectPin[] = "incorrect-pin";
const char NetworkDeviceHandler::kErrorNotSupported[] = "not-supported";
const char NetworkDeviceHandler::kErrorPinBlocked[] = "pin-blocked";
const char NetworkDeviceHandler::kErrorPinRequired[] = "pin-required";
const char NetworkDeviceHandler::kErrorUnknown[] = "unknown";

NetworkDeviceHandler::NetworkDeviceHandler() {
}

NetworkDeviceHandler::~NetworkDeviceHandler() {
}

void NetworkDeviceHandler::GetDeviceProperties(
    const std::string& device_path,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) const {
  DBusThreadManager::Get()->GetShillDeviceClient()->GetProperties(
      dbus::ObjectPath(device_path),
      base::Bind(&network_handler::GetPropertiesCallback,
                 callback, error_callback, device_path));
}

void NetworkDeviceHandler::SetDeviceProperty(
    const std::string& device_path,
    const std::string& name,
    const base::Value& value,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->SetProperty(
      dbus::ObjectPath(device_path),
      name,
      value,
      callback,
      base::Bind(&HandleShillCallFailure, device_path, error_callback));
}

void NetworkDeviceHandler::RequestRefreshIPConfigs(
    const std::string& device_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  GetDeviceProperties(device_path,
                      base::Bind(&RefreshIPConfigsCallback,
                                 callback, error_callback),
                      error_callback);
}

void NetworkDeviceHandler::ProposeScan(
    const std::string& device_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->ProposeScan(
      dbus::ObjectPath(device_path),
      base::Bind(&ProposeScanCallback, device_path, callback, error_callback));
}

void NetworkDeviceHandler::RegisterCellularNetwork(
    const std::string& device_path,
    const std::string& network_id,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->Register(
      dbus::ObjectPath(device_path),
      network_id,
      callback,
      base::Bind(&HandleShillCallFailure, device_path, error_callback));
}

void NetworkDeviceHandler::SetCarrier(
    const std::string& device_path,
    const std::string& carrier,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->SetCarrier(
      dbus::ObjectPath(device_path),
      carrier,
      callback,
      base::Bind(&HandleShillCallFailure, device_path, error_callback));
}

void NetworkDeviceHandler::RequirePin(
    const std::string& device_path,
    bool require_pin,
    const std::string& pin,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->RequirePin(
      dbus::ObjectPath(device_path),
      pin,
      require_pin,
      callback,
      base::Bind(&HandleShillCallFailure, device_path, error_callback));
}

void NetworkDeviceHandler::EnterPin(
    const std::string& device_path,
    const std::string& pin,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->EnterPin(
      dbus::ObjectPath(device_path),
      pin,
      callback,
      base::Bind(&HandleShillCallFailure, device_path, error_callback));
}

void NetworkDeviceHandler::UnblockPin(
    const std::string& device_path,
    const std::string& puk,
    const std::string& new_pin,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->UnblockPin(
      dbus::ObjectPath(device_path),
      puk,
      new_pin,
      callback,
      base::Bind(&HandleShillCallFailure, device_path, error_callback));
}

void NetworkDeviceHandler::ChangePin(
    const std::string& device_path,
    const std::string& old_pin,
    const std::string& new_pin,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->ChangePin(
      dbus::ObjectPath(device_path),
      old_pin,
      new_pin,
      callback,
      base::Bind(&HandleShillCallFailure, device_path, error_callback));
}

void NetworkDeviceHandler::HandleShillCallFailureForTest(
    const std::string& device_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& shill_error_name,
    const std::string& shill_error_message) {
  HandleShillCallFailure(
      device_path, error_callback, shill_error_name, shill_error_message);
}

}  // namespace chromeos
