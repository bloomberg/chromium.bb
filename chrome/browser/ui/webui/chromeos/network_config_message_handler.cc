// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/network_config_message_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/shill_property_util.h"
#include "components/onc/onc_constants.h"
#include "content/public/browser/web_ui.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

bool GetServicePathFromGuid(const std::string& guid,
                            std::string* service_path) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  if (!network)
    return false;
  *service_path = network->path();
  return true;
}

}  // namespace

NetworkConfigMessageHandler::NetworkConfigMessageHandler()
    : weak_ptr_factory_(this) {
}

NetworkConfigMessageHandler::~NetworkConfigMessageHandler() {
}

void NetworkConfigMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "networkConfig.getNetworks",
      base::Bind(&NetworkConfigMessageHandler::GetNetworks,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "networkConfig.getProperties",
      base::Bind(&NetworkConfigMessageHandler::GetProperties,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "networkConfig.getManagedProperties",
      base::Bind(&NetworkConfigMessageHandler::GetManagedProperties,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "networkConfig.getShillProperties",
      base::Bind(&NetworkConfigMessageHandler::GetShillProperties,
                 base::Unretained(this)));
}

void NetworkConfigMessageHandler::GetNetworks(
    const base::ListValue* arg_list) const {
  int callback_id = 0;
  const base::DictionaryValue* filter = NULL;
  if (!arg_list->GetInteger(0, &callback_id) ||
      !arg_list->GetDictionary(1, &filter)) {
    NOTREACHED();
  }
  std::string type = ::onc::network_type::kAllTypes;
  bool visible_only = false;
  bool configured_only = false;
  int limit = 1000;
  filter->GetString("type", &type);
  NetworkTypePattern pattern = onc::NetworkTypePatternFromOncType(type);
  filter->GetBoolean("visible", &visible_only);
  filter->GetBoolean("configured", &configured_only);
  filter->GetInteger("limit", &limit);

  base::ListValue return_arg_list;
  return_arg_list.AppendInteger(callback_id);

  scoped_ptr<base::ListValue> network_properties_list =
      chromeos::network_util::TranslateNetworkListToONC(
          pattern, configured_only, visible_only, limit,
          true /* debugging_properties */);

  return_arg_list.Append(network_properties_list.release());

  InvokeCallback(return_arg_list);
}

void NetworkConfigMessageHandler::GetProperties(
    const base::ListValue* arg_list) {
  int callback_id = 0;
  std::string guid;
  if (!arg_list->GetInteger(0, &callback_id) ||
      !arg_list->GetString(1, &guid)) {
    NOTREACHED();
  }
  std::string service_path;
  if (!GetServicePathFromGuid(guid, &service_path)) {
    scoped_ptr<base::DictionaryValue> error_data;
    ErrorCallback(callback_id, "Error.InvalidNetworkGuid", error_data.Pass());
    return;
  }
  NetworkHandler::Get()->managed_network_configuration_handler()->GetProperties(
      service_path,
      base::Bind(&NetworkConfigMessageHandler::GetPropertiesSuccess,
                 weak_ptr_factory_.GetWeakPtr(), callback_id),
      base::Bind(&NetworkConfigMessageHandler::ErrorCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void NetworkConfigMessageHandler::GetManagedProperties(
    const base::ListValue* arg_list) {
  int callback_id = 0;
  std::string guid;
  if (!arg_list->GetInteger(0, &callback_id) ||
      !arg_list->GetString(1, &guid)) {
    NOTREACHED();
  }
  std::string service_path;
  if (!GetServicePathFromGuid(guid, &service_path)) {
    scoped_ptr<base::DictionaryValue> error_data;
    ErrorCallback(callback_id, "Error.InvalidNetworkGuid", error_data.Pass());
    return;
  }
  NetworkHandler::Get()->managed_network_configuration_handler()->
      GetManagedProperties(
          LoginState::Get()->primary_user_hash(),
          service_path,
          base::Bind(&NetworkConfigMessageHandler::GetPropertiesSuccess,
                     weak_ptr_factory_.GetWeakPtr(), callback_id),
          base::Bind(&NetworkConfigMessageHandler::ErrorCallback,
                     weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void NetworkConfigMessageHandler::GetPropertiesSuccess(
    int callback_id,
    const std::string& service_path,
    const base::DictionaryValue& dictionary) const {
  base::ListValue return_arg_list;
  return_arg_list.AppendInteger(callback_id);

  base::DictionaryValue* network_properties = dictionary.DeepCopy();
  // Set the 'ServicePath' property for debugging.
  network_properties->SetStringWithoutPathExpansion(
      "ServicePath", service_path);

  return_arg_list.Append(network_properties);
  InvokeCallback(return_arg_list);
}

void NetworkConfigMessageHandler::GetShillProperties(
    const base::ListValue* arg_list) {
  int callback_id = 0;
  std::string guid;
  if (!arg_list->GetInteger(0, &callback_id) ||
      !arg_list->GetString(1, &guid)) {
    NOTREACHED();
  }
  std::string service_path;
  if (!GetServicePathFromGuid(guid, &service_path)) {
    scoped_ptr<base::DictionaryValue> error_data;
    ErrorCallback(callback_id, "Error.InvalidNetworkGuid", error_data.Pass());
    return;
  }
  NetworkHandler::Get()->network_configuration_handler()->GetProperties(
      service_path,
      base::Bind(&NetworkConfigMessageHandler::GetShillPropertiesSuccess,
                 weak_ptr_factory_.GetWeakPtr(), callback_id),
      base::Bind(&NetworkConfigMessageHandler::ErrorCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void NetworkConfigMessageHandler::GetShillPropertiesSuccess(
    int callback_id,
    const std::string& service_path,
    const base::DictionaryValue& dictionary) const {
  scoped_ptr<base::DictionaryValue> dictionary_copy(dictionary.DeepCopy());
  // Set the 'ServicePath' property for debugging.
  dictionary_copy->SetStringWithoutPathExpansion("ServicePath", service_path);

  // Get the device properties for debugging.
  std::string device;
  dictionary_copy->GetStringWithoutPathExpansion(
      shill::kDeviceProperty, &device);
  const DeviceState* device_state =
      NetworkHandler::Get()->network_state_handler()->GetDeviceState(device);
  if (device_state) {
    base::DictionaryValue* device_dictionary =
        device_state->properties().DeepCopy();
    dictionary_copy->Set(shill::kDeviceProperty, device_dictionary);

    // Convert IPConfig dictionary to a ListValue.
    base::ListValue* ip_configs = new base::ListValue;
    for (base::DictionaryValue::Iterator iter(device_state->ip_configs());
         !iter.IsAtEnd(); iter.Advance()) {
      ip_configs->Append(iter.value().DeepCopy());
    }
    device_dictionary->SetWithoutPathExpansion(
        shill::kIPConfigsProperty, ip_configs);
  }

  base::ListValue return_arg_list;
  return_arg_list.AppendInteger(callback_id);
  return_arg_list.Append(dictionary_copy.release());
  InvokeCallback(return_arg_list);
}

void NetworkConfigMessageHandler::InvokeCallback(
    const base::ListValue& arg_list) const {
  web_ui()->CallJavascriptFunction(
      "networkConfig.chromeCallbackSuccess", arg_list);
}

void NetworkConfigMessageHandler::ErrorCallback(
    int callback_id,
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) const {
  LOG(ERROR) << "NetworkConfigMessageHandler Error: " << error_name;
  base::ListValue arg_list;
  arg_list.AppendInteger(callback_id);
  arg_list.AppendString(error_name);
  web_ui()->CallJavascriptFunction(
      "networkConfig.chromeCallbackError", arg_list);
}

}  // namespace chromeos
