// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_device_handler_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/shill_property_util.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

std::string GetErrorNameForShillError(const std::string& shill_error_name) {
  if (shill_error_name == shill::kErrorResultFailure)
    return NetworkDeviceHandler::kErrorFailure;
  if (shill_error_name == shill::kErrorResultNotSupported)
    return NetworkDeviceHandler::kErrorNotSupported;
  if (shill_error_name == shill::kErrorResultIncorrectPin)
    return NetworkDeviceHandler::kErrorIncorrectPin;
  if (shill_error_name == shill::kErrorResultPinBlocked)
    return NetworkDeviceHandler::kErrorPinBlocked;
  if (shill_error_name == shill::kErrorResultPinRequired)
    return NetworkDeviceHandler::kErrorPinRequired;
  return NetworkDeviceHandler::kErrorUnknown;
}

void InvokeErrorCallback(const std::string& service_path,
                         const network_handler::ErrorCallback& error_callback,
                         const std::string& error_name) {
  std::string error_msg = "Device Error: " + error_name;
  NET_LOG_ERROR(error_msg, service_path);
  network_handler::RunErrorCallback(
      error_callback, service_path, error_name, error_msg);
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
  const base::ListValue* ip_configs;
  if (!properties.GetListWithoutPathExpansion(
          shill::kIPConfigsProperty, &ip_configs)) {
    NET_LOG_ERROR("RequestRefreshIPConfigs Failed", device_path);
    network_handler::ShillErrorCallbackFunction(
        "RequestRefreshIPConfigs Failed",
        device_path,
        error_callback,
        std::string("Missing ") + shill::kIPConfigsProperty, "");
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

void SetDevicePropertyInternal(
    const std::string& device_path,
    const std::string& property_name,
    const base::Value& value,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->SetProperty(
      dbus::ObjectPath(device_path),
      property_name,
      value,
      callback,
      base::Bind(&HandleShillCallFailure, device_path, error_callback));
}

}  // namespace

NetworkDeviceHandlerImpl::~NetworkDeviceHandlerImpl() {
  network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void NetworkDeviceHandlerImpl::GetDeviceProperties(
    const std::string& device_path,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) const {
  DBusThreadManager::Get()->GetShillDeviceClient()->GetProperties(
      dbus::ObjectPath(device_path),
      base::Bind(&network_handler::GetPropertiesCallback,
                 callback, error_callback, device_path));
}

void NetworkDeviceHandlerImpl::SetDeviceProperty(
    const std::string& device_path,
    const std::string& property_name,
    const base::Value& value,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  const char* const property_blacklist[] = {
      // Must only be changed by policy/owner through.
      shill::kCellularAllowRoamingProperty
  };

  for (size_t i = 0; i < arraysize(property_blacklist); ++i) {
    if (property_name == property_blacklist[i]) {
      InvokeErrorCallback(
          device_path,
          error_callback,
          "SetDeviceProperty called on blacklisted property " + property_name);
      return;
    }
  }

  SetDevicePropertyInternal(
      device_path, property_name, value, callback, error_callback);
}

void NetworkDeviceHandlerImpl::RequestRefreshIPConfigs(
    const std::string& device_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  GetDeviceProperties(device_path,
                      base::Bind(&RefreshIPConfigsCallback,
                                 callback, error_callback),
                      error_callback);
}

void NetworkDeviceHandlerImpl::ProposeScan(
    const std::string& device_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetShillDeviceClient()->ProposeScan(
      dbus::ObjectPath(device_path),
      base::Bind(&ProposeScanCallback, device_path, callback, error_callback));
}

void NetworkDeviceHandlerImpl::RegisterCellularNetwork(
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

void NetworkDeviceHandlerImpl::SetCarrier(
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

void NetworkDeviceHandlerImpl::RequirePin(
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

void NetworkDeviceHandlerImpl::EnterPin(
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

void NetworkDeviceHandlerImpl::UnblockPin(
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

void NetworkDeviceHandlerImpl::ChangePin(
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

void NetworkDeviceHandlerImpl::SetCellularAllowRoaming(
    const bool allow_roaming) {
  cellular_allow_roaming_ = allow_roaming;
  ApplyCellularAllowRoamingToShill();
}

void NetworkDeviceHandlerImpl::DeviceListChanged() {
  ApplyCellularAllowRoamingToShill();
}

NetworkDeviceHandlerImpl::NetworkDeviceHandlerImpl()
    : network_state_handler_(NULL),
      cellular_allow_roaming_(false) {}

void NetworkDeviceHandlerImpl::Init(
    NetworkStateHandler* network_state_handler) {
  DCHECK(network_state_handler);
  network_state_handler_ = network_state_handler;
  network_state_handler_->AddObserver(this, FROM_HERE);
}

void NetworkDeviceHandlerImpl::ApplyCellularAllowRoamingToShill() {
  NetworkStateHandler::DeviceStateList list;
  network_state_handler_->GetDeviceListByType(NetworkTypePattern::Cellular(),
                                              &list);
  if (list.empty()) {
    NET_LOG_DEBUG("No cellular device is available",
                  "Roaming is only supported by cellular devices.");
    return;
  }
  for (NetworkStateHandler::DeviceStateList::const_iterator it = list.begin();
      it != list.end(); ++it) {
    const DeviceState* device_state = *it;
    bool current_device_value;
    if (!device_state->properties().GetBooleanWithoutPathExpansion(
             shill::kCellularAllowRoamingProperty, &current_device_value)) {
      NET_LOG_ERROR(
          "Could not get \"allow roaming\" property from cellular "
          "device.",
          device_state->path());
      continue;
    }

    // If roaming is required by the provider, always try to set to true.
    bool new_device_value =
        device_state->provider_requires_roaming() || cellular_allow_roaming_;

    // Only set the value if the current value is different from
    // |new_device_value|.
    if (new_device_value == current_device_value)
      continue;

    SetDevicePropertyInternal(device_state->path(),
                              shill::kCellularAllowRoamingProperty,
                              base::FundamentalValue(new_device_value),
                              base::Bind(&base::DoNothing),
                              network_handler::ErrorCallback());
  }
}

}  // namespace chromeos
