// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_device_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
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

base::DictionaryValue* CreateDeviceErrorData(const std::string& device_path,
                                             const std::string& error_name,
                                             const std::string& error_message) {
  // Create error data with no 'servicePath' entry.
  base::DictionaryValue* error_data =
      network_handler::CreateErrorData("", error_name, error_message);
  if (!device_path.empty())
    error_data->SetString(kDevicePath, device_path);
  return error_data;
}

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

void NetworkErrorCallbackFunction(
    const std::string& path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_description) {
  NET_LOG_ERROR(error_description, path);
  if (error_callback.is_null())
    return;
  scoped_ptr<base::DictionaryValue> error_data(
      CreateDeviceErrorData(path, error_name, error_description));
  error_callback.Run(error_name, error_data.Pass());
}

void ShillErrorCallbackFunction(
    const std::string& path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& shill_error_name,
    const std::string& shill_error_message) {
  std::string description = "Shill Error: name=" + shill_error_name +
                            ", message=\"" + shill_error_message + "\"";
  NetworkErrorCallbackFunction(path, error_callback, error_name, description);
}

void InvokeShillErrorCallback(
    const std::string& path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& shill_error_name,
    const std::string& shill_error_message) {
  std::string error_name = GetErrorNameForShillError(shill_error_name);
  ShillErrorCallbackFunction(path,
                             error_callback,
                             error_name,
                             shill_error_name,
                             shill_error_message);
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

void NetworkDeviceHandler::SetCarrier(
    const std::string& device_path,
    const std::string& carrier,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->SetCarrier(
      dbus::ObjectPath(device_path),
      carrier,
      callback,
      base::Bind(&NetworkDeviceHandler::HandleShillCallFailure,
                 AsWeakPtr(), device_path, error_callback));
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
      base::Bind(&NetworkDeviceHandler::HandleShillCallFailure,
                 AsWeakPtr(), device_path, error_callback));
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
      base::Bind(&NetworkDeviceHandler::HandleShillCallFailure,
                 AsWeakPtr(), device_path, error_callback));
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
      base::Bind(&NetworkDeviceHandler::HandleShillCallFailure,
                 AsWeakPtr(), device_path, error_callback));
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
      base::Bind(&NetworkDeviceHandler::HandleShillCallFailure,
                 AsWeakPtr(), device_path, error_callback));
}

void NetworkDeviceHandler::HandleShillCallFailure(
    const std::string& device_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& shill_error_name,
    const std::string& shill_error_message) {
  InvokeShillErrorCallback(
      device_path, error_callback, shill_error_name, shill_error_message);
}

}  // namespace chromeos
