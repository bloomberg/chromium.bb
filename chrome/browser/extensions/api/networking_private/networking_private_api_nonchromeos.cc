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
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/networking_private.h"
#include "components/onc/onc_constants.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_system.h"

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

  NetworkingPrivateServiceClient* service_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());

  service_client->GetProperties(
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
  NetworkingPrivateServiceClient* service_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());

  service_client->GetManagedProperties(
      params->network_guid,
      base::Bind(&NetworkingPrivateGetManagedPropertiesFunction::Success,
                 this),
      base::Bind(&NetworkingPrivateGetManagedPropertiesFunction::Failure,
                 this));
  return true;
}

void NetworkingPrivateGetManagedPropertiesFunction::Success(
    const std::string& network_guid,
    const base::DictionaryValue& dictionary) {
  scoped_ptr<base::DictionaryValue> network_properties(dictionary.DeepCopy());
  network_properties->SetStringWithoutPathExpansion(onc::network_config::kGUID,
                                                    network_guid);
  SetResult(network_properties.release());
  SendResponse(true);
}

void NetworkingPrivateGetManagedPropertiesFunction::Failure(
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
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
  NetworkingPrivateServiceClient* service_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());

  service_client->GetState(
      params->network_guid,
      base::Bind(&NetworkingPrivateGetStateFunction::Success,
                 this),
      base::Bind(&NetworkingPrivateGetStateFunction::Failure,
                 this));
  return true;
}

void NetworkingPrivateGetStateFunction::Success(
    const std::string& network_guid,
    const base::DictionaryValue& dictionary) {
  SetResult(dictionary.DeepCopy());
  SendResponse(true);
}

void NetworkingPrivateGetStateFunction::Failure(
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
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

  NetworkingPrivateServiceClient* service_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());

  service_client->SetProperties(
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
  scoped_ptr<base::DictionaryValue> properties_dict(
      params->properties.ToValue());

  NetworkingPrivateServiceClient* service_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());

  service_client->CreateNetwork(
      params->shared,
      *properties_dict,
      base::Bind(&NetworkingPrivateCreateNetworkFunction::ResultCallback, this),
      base::Bind(&NetworkingPrivateCreateNetworkFunction::ErrorCallback, this));
  return true;
}

void NetworkingPrivateCreateNetworkFunction::ErrorCallback(
    const std::string& error_name,
    const scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
}

void NetworkingPrivateCreateNetworkFunction::ResultCallback(
    const std::string& guid) {
  results_ = api::CreateNetwork::Results::Create(guid);
  SendResponse(true);
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

  NetworkingPrivateServiceClient* service_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());

  service_client->GetVisibleNetworks(
      api::GetVisibleNetworks::Params::ToString(params->type),
      base::Bind(&NetworkingPrivateGetVisibleNetworksFunction::ResultCallback,
                 this));

  return true;
}

void NetworkingPrivateGetVisibleNetworksFunction::ResultCallback(
    const base::ListValue& network_list) {
  SetResult(network_list.DeepCopy());
  SendResponse(true);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetEnabledNetworkTypesFunction

NetworkingPrivateGetEnabledNetworkTypesFunction::
~NetworkingPrivateGetEnabledNetworkTypesFunction() {
}

bool NetworkingPrivateGetEnabledNetworkTypesFunction::RunImpl() {
  base::ListValue* network_list = new base::ListValue;
  network_list->Append(new base::StringValue(onc::network_type::kWiFi));
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
  NetworkingPrivateServiceClient* service_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());
  service_client->RequestNetworkScan();
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
  NetworkingPrivateServiceClient* service_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());
  service_client->StartConnect(
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
  NetworkingPrivateServiceClient* service_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());
  service_client->StartDisconnect(
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
  NetworkingPrivateServiceClient* service_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());
  service_client->VerifyDestination(
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
  NetworkingPrivateServiceClient* service_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());
  service_client->VerifyAndEncryptCredentials(
      args_.Pass(),
      base::Bind(
          &NetworkingPrivateVerifyAndEncryptCredentialsFunction::ResultCallback,
          this),
      base::Bind(
          &NetworkingPrivateVerifyAndEncryptCredentialsFunction::ErrorCallback,
          this));
  return true;
}

void NetworkingPrivateVerifyAndEncryptCredentialsFunction::ResultCallback(
    const std::string& result) {
  SetResult(new base::StringValue(result));
  SendResponse(true);
}

void NetworkingPrivateVerifyAndEncryptCredentialsFunction::ErrorCallback(
    const std::string& error_name,
    const std::string& error) {
  error_ = error_name;
  SendResponse(false);
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
  NetworkingPrivateServiceClient* service_client =
      NetworkingPrivateServiceClientFactory::GetForProfile(GetProfile());
  service_client->VerifyAndEncryptData(
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

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSetWifiTDLSEnabledStateFunction

NetworkingPrivateSetWifiTDLSEnabledStateFunction::
  ~NetworkingPrivateSetWifiTDLSEnabledStateFunction() {
}

bool NetworkingPrivateSetWifiTDLSEnabledStateFunction::RunImpl() {
  scoped_ptr<api::SetWifiTDLSEnabledState::Params> params =
      api::SetWifiTDLSEnabledState::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  SetError("not-implemented");
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetWifiTDLSStatusFunction

NetworkingPrivateGetWifiTDLSStatusFunction::
  ~NetworkingPrivateGetWifiTDLSStatusFunction() {
}

bool NetworkingPrivateGetWifiTDLSStatusFunction::RunImpl() {
  scoped_ptr<api::GetWifiTDLSStatus::Params> params =
      api::GetWifiTDLSStatus::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  SetError("not-implemented");
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetCaptivePortalStatusFunction

NetworkingPrivateGetCaptivePortalStatusFunction::
    ~NetworkingPrivateGetCaptivePortalStatusFunction() {}

bool NetworkingPrivateGetCaptivePortalStatusFunction::RunImpl() {
  scoped_ptr<api::GetCaptivePortalStatus::Params> params =
      api::GetCaptivePortalStatus::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  SetError("not-implemented");
  return false;
}
