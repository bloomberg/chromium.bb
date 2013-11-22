// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_api.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_service_client.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_service_client_factory.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/networking_private.h"
#include "components/onc/onc_constants.h"
#include "extensions/browser/event_router.h"

using extensions::NetworkingPrivateServiceClient;
using extensions::NetworkingPrivateServiceClientFactory;
namespace api = extensions::api::networking_private;

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetPropertiesFunction

NetworkingPrivateGetPropertiesFunction::
  ~NetworkingPrivateGetPropertiesFunction() {
}

bool NetworkingPrivateGetPropertiesFunction::RunImpl() {
  scoped_ptr<api::GetProperties::Params> params =
      api::GetProperties::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  NetworkingPrivateServiceClient* process_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());

  process_client->GetProperties(
      params->network_guid,
      base::Bind(&NetworkingPrivateGetPropertiesFunction::GetPropertiesSuccess,
                 this),
      base::Bind(&NetworkingPrivateGetPropertiesFunction::GetPropertiesFailed,
                 this));
  return true;
}

void NetworkingPrivateGetPropertiesFunction::GetPropertiesSuccess(
    const std::string& network_guid,
    const base::DictionaryValue& dictionary) {
  SetResult(dictionary.DeepCopy());
  SendResponse(true);
}

void NetworkingPrivateGetPropertiesFunction::GetPropertiesFailed(
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetManagedPropertiesFunction

NetworkingPrivateGetManagedPropertiesFunction::
  ~NetworkingPrivateGetManagedPropertiesFunction() {
}

bool NetworkingPrivateGetManagedPropertiesFunction::RunImpl() {
  scoped_ptr<api::GetManagedProperties::Params> params =
      api::GetManagedProperties::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  const std::string network_properties =
      "{"
      "  \"ConnectionState\": {"
      "    \"Active\": \"NotConnected\","
      "    \"Effective\": \"Unmanaged\""
      "  },"
      "  \"GUID\": \"stub_wifi2\","
      "  \"Name\": {"
      "    \"Active\": \"wifi2_PSK\","
      "    \"Effective\": \"UserPolicy\","
      "    \"UserPolicy\": \"My WiFi Network\""
      "  },"
      "  \"Type\": {"
      "    \"Active\": \"WiFi\","
      "    \"Effective\": \"UserPolicy\","
      "    \"UserPolicy\": \"WiFi\""
      "  },"
      "  \"WiFi\": {"
      "    \"AutoConnect\": {"
      "      \"Active\": false,"
      "      \"UserEditable\": true"
      "    },"
      "    \"Frequency\" : {"
      "      \"Active\": 5000,"
      "      \"Effective\": \"Unmanaged\""
      "    },"
      "    \"FrequencyList\" : {"
      "      \"Active\": [2400, 5000],"
      "      \"Effective\": \"Unmanaged\""
      "    },"
      "    \"Passphrase\": {"
      "      \"Effective\": \"UserSetting\","
      "      \"UserEditable\": true,"
      "      \"UserSetting\": \"FAKE_CREDENTIAL_VPaJDV9x\""
      "    },"
      "    \"SSID\": {"
      "      \"Active\": \"wifi2_PSK\","
      "      \"Effective\": \"UserPolicy\","
      "      \"UserPolicy\": \"wifi2_PSK\""
      "    },"
      "    \"Security\": {"
      "      \"Active\": \"WPA-PSK\","
      "      \"Effective\": \"UserPolicy\","
      "      \"UserPolicy\": \"WPA-PSK\""
      "    },"
      "    \"SignalStrength\": {"
      "      \"Active\": 80,"
      "      \"Effective\": \"Unmanaged\""
      "    }"
      "  }"
      "}";

  SetResult(base::JSONReader::Read(network_properties));
  SendResponse(true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetStateFunction

NetworkingPrivateGetStateFunction::
  ~NetworkingPrivateGetStateFunction() {
}

bool NetworkingPrivateGetStateFunction::RunImpl() {
  scoped_ptr<api::GetState::Params> params =
      api::GetState::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  const std::string network_state =
      "{"
      "  \"ConnectionState\": \"NotConnected\","
      "  \"GUID\": \"stub_wifi2\","
      "  \"Name\": \"wifi2_PSK\","
      "  \"Type\": \"WiFi\","
      "  \"WiFi\": {"
      "    \"Security\": \"WPA-PSK\","
      "    \"SignalStrength\": 80"
      "  }"
      "}";
  SetResult(base::JSONReader::Read(network_state));
  SendResponse(true);
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSetPropertiesFunction

NetworkingPrivateSetPropertiesFunction::
  ~NetworkingPrivateSetPropertiesFunction() {
}

bool NetworkingPrivateSetPropertiesFunction::RunImpl() {
  scoped_ptr<api::SetProperties::Params> params =
      api::SetProperties::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  scoped_ptr<base::DictionaryValue> properties_dict(
      params->properties.ToValue());

  NetworkingPrivateServiceClient* process_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());

  process_client->SetProperties(
      params->network_guid,
      *properties_dict,
      base::Bind(&NetworkingPrivateSetPropertiesFunction::ResultCallback, this),
      base::Bind(&NetworkingPrivateSetPropertiesFunction::ErrorCallback, this));
  return true;
}

void NetworkingPrivateSetPropertiesFunction::ResultCallback() {
  SendResponse(true);
}

void NetworkingPrivateSetPropertiesFunction::ErrorCallback(
    const std::string& error_name,
    const scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateCreateNetworkFunction

NetworkingPrivateCreateNetworkFunction::
~NetworkingPrivateCreateNetworkFunction() {
}

bool NetworkingPrivateCreateNetworkFunction::RunImpl() {
  scoped_ptr<api::CreateNetwork::Params> params =
      api::CreateNetwork::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  results_ = api::CreateNetwork::Results::Create("fake_guid");
  SendResponse(true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetVisibleNetworksFunction

NetworkingPrivateGetVisibleNetworksFunction::
  ~NetworkingPrivateGetVisibleNetworksFunction() {
}

bool NetworkingPrivateGetVisibleNetworksFunction::RunImpl() {
  scoped_ptr<api::GetVisibleNetworks::Params> params =
      api::GetVisibleNetworks::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  NetworkingPrivateServiceClient* process_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());

  process_client->GetVisibleNetworks(base::Bind(
      &NetworkingPrivateGetVisibleNetworksFunction::ResultCallback, this));

  return true;
}

void NetworkingPrivateGetVisibleNetworksFunction::ResultCallback(
    const base::ListValue& network_list) {
  scoped_ptr<api::GetVisibleNetworks::Params> params =
      api::GetVisibleNetworks::Params::Create(*args_);
  ListValue* result = new ListValue();
  std::string params_type =
      api::GetVisibleNetworks::Params::ToString(params->type);
  bool request_all = params->type == api::GetVisibleNetworks::Params::TYPE_ALL;

  // Copy networks of requested type;
  for (base::ListValue::const_iterator it = network_list.begin();
       it != network_list.end();
       ++it) {
    const base::Value* network_value = *it;
    const DictionaryValue* network_dict = NULL;
    if (!network_value->GetAsDictionary(&network_dict)) {
      LOG(ERROR) << "Value is not a dictionary";
      continue;
    }
    if (!request_all) {
      std::string network_type;
      network_dict->GetString(onc::network_config::kType, &network_type);
      if (network_type != params_type)
        continue;
    }
    result->Append(network_value->DeepCopy());
  }

  SetResult(result);
  SendResponse(true);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetEnabledNetworkTypesFunction

NetworkingPrivateGetEnabledNetworkTypesFunction::
~NetworkingPrivateGetEnabledNetworkTypesFunction() {
}

bool NetworkingPrivateGetEnabledNetworkTypesFunction::RunImpl() {
  base::ListValue* network_list = new base::ListValue;

  network_list->Append(new base::StringValue("Ethernet"));
  network_list->Append(new base::StringValue("WiFi"));
  network_list->Append(new base::StringValue("Cellular"));

  SetResult(network_list);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateEnableNetworkTypeFunction

NetworkingPrivateEnableNetworkTypeFunction::
~NetworkingPrivateEnableNetworkTypeFunction() {
}

bool NetworkingPrivateEnableNetworkTypeFunction::RunImpl() {
  scoped_ptr<api::EnableNetworkType::Params> params =
      api::EnableNetworkType::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateDisableNetworkTypeFunction

NetworkingPrivateDisableNetworkTypeFunction::
~NetworkingPrivateDisableNetworkTypeFunction() {
}

bool NetworkingPrivateDisableNetworkTypeFunction::RunImpl() {
  scoped_ptr<api::DisableNetworkType::Params> params =
      api::DisableNetworkType::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateRequestNetworkScanFunction

NetworkingPrivateRequestNetworkScanFunction::
  ~NetworkingPrivateRequestNetworkScanFunction() {
}

bool NetworkingPrivateRequestNetworkScanFunction::RunImpl() {
  NetworkingPrivateServiceClient* process_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());
  process_client->RequestNetworkScan();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartConnectFunction

NetworkingPrivateStartConnectFunction::
  ~NetworkingPrivateStartConnectFunction() {
}

bool NetworkingPrivateStartConnectFunction::RunImpl() {
  scoped_ptr<api::StartConnect::Params> params =
      api::StartConnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  NetworkingPrivateServiceClient* process_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());
  process_client->StartConnect(
      params->network_guid,
      base::Bind(&NetworkingPrivateStartConnectFunction::ConnectionStartSuccess,
                 this),
      base::Bind(&NetworkingPrivateStartConnectFunction::ConnectionStartFailed,
                 this));
  return true;
}

void NetworkingPrivateStartConnectFunction::ConnectionStartSuccess() {
  SendResponse(true);
}

void NetworkingPrivateStartConnectFunction::ConnectionStartFailed(
    const std::string& error_name,
    const scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartDisconnectFunction

NetworkingPrivateStartDisconnectFunction::
  ~NetworkingPrivateStartDisconnectFunction() {
}

bool NetworkingPrivateStartDisconnectFunction::RunImpl() {
  scoped_ptr<api::StartDisconnect::Params> params =
      api::StartDisconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  NetworkingPrivateServiceClient* process_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());
  process_client->StartDisconnect(
      params->network_guid,
      base::Bind(
          &NetworkingPrivateStartDisconnectFunction::DisconnectionStartSuccess,
          this),
      base::Bind(
          &NetworkingPrivateStartDisconnectFunction::DisconnectionStartFailed,
          this));
  return true;
}

void NetworkingPrivateStartDisconnectFunction::DisconnectionStartSuccess() {
  SendResponse(true);
}

void NetworkingPrivateStartDisconnectFunction::DisconnectionStartFailed(
    const std::string& error_name,
    const scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyDestinationFunction

NetworkingPrivateVerifyDestinationFunction::
    ~NetworkingPrivateVerifyDestinationFunction() {}

bool NetworkingPrivateVerifyDestinationFunction::RunImpl() {
  scoped_ptr<api::VerifyDestination::Params> params =
      api::VerifyDestination::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  NetworkingPrivateServiceClient* process_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());
  process_client->VerifyDestination(
      args_.Pass(),
      base::Bind(
          &NetworkingPrivateVerifyDestinationFunction::ResultCallback,
          this),
      base::Bind(
          &NetworkingPrivateVerifyDestinationFunction::ErrorCallback,
          this));
  return true;
}

void NetworkingPrivateVerifyDestinationFunction::ResultCallback(bool result) {
  SetResult(new base::FundamentalValue(result));
  SendResponse(true);
}

void NetworkingPrivateVerifyDestinationFunction::ErrorCallback(
    const std::string& error_name, const std::string& error) {
  error_ = error_name;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyAndEncryptCredentialsFunction

NetworkingPrivateVerifyAndEncryptCredentialsFunction::
  ~NetworkingPrivateVerifyAndEncryptCredentialsFunction() {
}

bool NetworkingPrivateVerifyAndEncryptCredentialsFunction::RunImpl() {
  scoped_ptr<api::VerifyAndEncryptCredentials::Params> params =
      api::VerifyAndEncryptCredentials::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  SetResult(new base::StringValue("encrypted_credentials"));
  SendResponse(true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyAndEncryptDataFunction

NetworkingPrivateVerifyAndEncryptDataFunction::
  ~NetworkingPrivateVerifyAndEncryptDataFunction() {
}

bool NetworkingPrivateVerifyAndEncryptDataFunction::RunImpl() {
  scoped_ptr<api::VerifyAndEncryptData::Params> params =
      api::VerifyAndEncryptData::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  NetworkingPrivateServiceClient* process_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());
  process_client->VerifyAndEncryptData(
      args_.Pass(),
      base::Bind(
          &NetworkingPrivateVerifyAndEncryptDataFunction::ResultCallback,
          this),
      base::Bind(
          &NetworkingPrivateVerifyAndEncryptDataFunction::ErrorCallback,
          this));
  return true;
}

void NetworkingPrivateVerifyAndEncryptDataFunction::ResultCallback(
    const std::string& result) {
  SetResult(new base::StringValue(result));
  SendResponse(true);
}

void NetworkingPrivateVerifyAndEncryptDataFunction::ErrorCallback(
    const std::string& error_name, const std::string& error) {
  error_ = error_name;
  SendResponse(false);
}
