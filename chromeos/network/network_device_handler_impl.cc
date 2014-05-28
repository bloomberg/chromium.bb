// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_device_handler_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state_handler.h"
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
  if (shill_error_name == shill::kErrorResultNotFound)
    return NetworkDeviceHandler::kErrorDeviceMissing;
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

// Struct containing TDLS Operation parameters.
struct TDLSOperationParams {
  TDLSOperationParams() : retry_count(0) {}
  std::string operation;
  std::string ip_or_mac_address;
  int retry_count;
};

// Forward declare for PostDelayedTask.
void CallPerformTDLSOperation(
    const std::string& device_path,
    const TDLSOperationParams& params,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback);

void TDLSSuccessCallback(
    const std::string& device_path,
    const TDLSOperationParams& params,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback,
    const std::string& result) {
  std::string event_desc = "TDLSSuccessCallback: " + params.operation;
  if (!result.empty())
    event_desc += ": " + result;
  NET_LOG_EVENT(event_desc, device_path);
  if (params.operation != shill::kTDLSSetupOperation) {
    if (!callback.is_null())
      callback.Run(result);
    return;
  }

  if (!result.empty())
    NET_LOG_ERROR("Unexpected TDLS result: " + result, device_path);

  // Send a delayed Status request after a successful Setup call.
  TDLSOperationParams status_params;
  status_params.operation = shill::kTDLSStatusOperation;
  status_params.ip_or_mac_address = params.ip_or_mac_address;

  const int64 kRequestStatusDelayMs = 500;
  base::TimeDelta request_delay;
  if (!DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface())
    request_delay = base::TimeDelta::FromMilliseconds(kRequestStatusDelayMs);

  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CallPerformTDLSOperation,
                 device_path, status_params, callback, error_callback),
      request_delay);
}

void TDLSErrorCallback(
    const std::string& device_path,
    const TDLSOperationParams& params,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  // If a Setup operation receives an InProgress error, retry.
  const int kMaxRetries = 5;
  if (params.operation == shill::kTDLSSetupOperation &&
      dbus_error_name == shill::kErrorResultInProgress &&
      params.retry_count < kMaxRetries) {
    TDLSOperationParams retry_params = params;
    ++retry_params.retry_count;
    NET_LOG_EVENT(base::StringPrintf("TDLS Retry: %d", params.retry_count),
                  device_path);
    const int64 kReRequestDelayMs = 1000;
    base::TimeDelta request_delay;
    if (!DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface())
      request_delay = base::TimeDelta::FromMilliseconds(kReRequestDelayMs);

    base::MessageLoopProxy::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CallPerformTDLSOperation,
                   device_path, retry_params, callback, error_callback),
        request_delay);
    return;
  }

  NET_LOG_ERROR("TDLS Error:" + dbus_error_name + ":" + dbus_error_message,
                device_path);
  if (error_callback.is_null())
    return;

  const std::string error_name =
      dbus_error_name == shill::kErrorResultInProgress ?
      NetworkDeviceHandler::kErrorTimeout : NetworkDeviceHandler::kErrorUnknown;
  const std::string& error_detail = params.ip_or_mac_address;
  scoped_ptr<base::DictionaryValue> error_data(
      network_handler::CreateDBusErrorData(
          device_path, error_name, error_detail,
          dbus_error_name, dbus_error_message));
  error_callback.Run(error_name, error_data.Pass());
}

void CallPerformTDLSOperation(
    const std::string& device_path,
    const TDLSOperationParams& params,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG_EVENT("CallPerformTDLSOperation: " + params.operation, device_path);
  DBusThreadManager::Get()->GetShillDeviceClient()->PerformTDLSOperation(
      dbus::ObjectPath(device_path),
      params.operation,
      params.ip_or_mac_address,
      base::Bind(&TDLSSuccessCallback,
                 device_path, params, callback, error_callback),
      base::Bind(&TDLSErrorCallback,
                 device_path, params, callback, error_callback));
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

void NetworkDeviceHandlerImpl::SetWifiTDLSEnabled(
    const std::string& ip_or_mac_address,
    bool enabled,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {
  const DeviceState* device_state =
      network_state_handler_->GetDeviceStateByType(NetworkTypePattern::WiFi());
  if (!device_state) {
    if (error_callback.is_null())
      return;
    scoped_ptr<base::DictionaryValue> error_data(new base::DictionaryValue);
    error_data->SetString(network_handler::kErrorName, kErrorDeviceMissing);
    error_callback.Run(kErrorDeviceMissing, error_data.Pass());
    return;
  }
  TDLSOperationParams params;
  params.operation = enabled ? shill::kTDLSSetupOperation
      : shill::kTDLSTeardownOperation;
  params.ip_or_mac_address = ip_or_mac_address;
  CallPerformTDLSOperation(
      device_state->path(), params, callback, error_callback);
}

void NetworkDeviceHandlerImpl::GetWifiTDLSStatus(
    const std::string& ip_or_mac_address,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) {
  const DeviceState* device_state =
      network_state_handler_->GetDeviceStateByType(NetworkTypePattern::WiFi());
  if (!device_state) {
    if (error_callback.is_null())
      return;
    scoped_ptr<base::DictionaryValue> error_data(new base::DictionaryValue);
    error_data->SetString(network_handler::kErrorName, kErrorDeviceMissing);
    error_callback.Run(kErrorDeviceMissing, error_data.Pass());
    return;
  }
  TDLSOperationParams params;
  params.operation = shill::kTDLSStatusOperation;
  params.ip_or_mac_address = ip_or_mac_address;
  CallPerformTDLSOperation(
      device_state->path(), params, callback, error_callback);
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
    bool current_allow_roaming = device_state->allow_roaming();

    // If roaming is required by the provider, always try to set to true.
    bool new_device_value =
        device_state->provider_requires_roaming() || cellular_allow_roaming_;

    // Only set the value if the current value is different from
    // |new_device_value|.
    if (new_device_value == current_allow_roaming)
      continue;

    SetDevicePropertyInternal(device_state->path(),
                              shill::kCellularAllowRoamingProperty,
                              base::FundamentalValue(new_device_value),
                              base::Bind(&base::DoNothing),
                              network_handler::ErrorCallback());
  }
}

}  // namespace chromeos
