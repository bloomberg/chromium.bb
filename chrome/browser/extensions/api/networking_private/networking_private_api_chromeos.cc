// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_api.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/extensions/api/networking_private.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/onc/onc_constants.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_function_registry.h"

namespace api = extensions::api::networking_private;

using chromeos::DBusThreadManager;
using chromeos::ManagedNetworkConfigurationHandler;
using chromeos::NetworkHandler;
using chromeos::NetworkPortalDetector;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;
using chromeos::ShillManagerClient;

namespace {

const int kDefaultNetworkListLimit = 1000;

// Helper function that converts between the two types of verification
// properties. They should always have the same fields, but we do this here to
// prevent ShillManagerClient from depending directly on the extension API.
ShillManagerClient::VerificationProperties ConvertVerificationProperties(
    const api::VerificationProperties& input) {
  ShillManagerClient::VerificationProperties output;
  COMPILE_ASSERT(sizeof(api::VerificationProperties) ==
                     sizeof(ShillManagerClient::VerificationProperties),
                 verification_properties_no_longer_match);

  output.certificate = input.certificate;
  output.public_key = input.public_key;
  output.nonce = input.nonce;
  output.signed_data = input.signed_data;
  output.device_serial = input.device_serial;
  output.device_ssid = input.device_ssid;
  output.device_bssid = input.device_bssid;
  return output;
}

bool GetUserIdHash(content::BrowserContext* browser_context,
                   std::string* user_hash) {
  // Currently Chrome OS only configures networks for the primary user.
  // Configuration attempts from other browser contexts should fail.
  // TODO(stevenjb): use an ExtensionsBrowserClient method to access
  // ProfileHelper when moving this to src/extensions.
  std::string current_user_hash =
      chromeos::ProfileHelper::GetUserIdHashFromProfile(
          static_cast<Profile*>(browser_context));

  if (current_user_hash != chromeos::LoginState::Get()->primary_user_hash())
    return false;
  *user_hash = current_user_hash;
  return true;
}

bool GetServicePathFromGuid(const std::string& guid,
                            std::string* service_path,
                            std::string* error) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  if (!network) {
    *error = "Error.InvalidNetworkGuid";
    return false;
  }
  *service_path = network->path();
  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetPropertiesFunction

NetworkingPrivateGetPropertiesFunction::
  ~NetworkingPrivateGetPropertiesFunction() {
}

bool NetworkingPrivateGetPropertiesFunction::RunAsync() {
  scoped_ptr<api::GetProperties::Params> params =
      api::GetProperties::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string service_path;
  if (!GetServicePathFromGuid(params->network_guid, &service_path, &error_))
    return false;

  NetworkHandler::Get()->managed_network_configuration_handler()->GetProperties(
      service_path,
      base::Bind(&NetworkingPrivateGetPropertiesFunction::GetPropertiesSuccess,
                 this),
      base::Bind(&NetworkingPrivateGetPropertiesFunction::GetPropertiesFailed,
                 this));
  return true;
}

void NetworkingPrivateGetPropertiesFunction::GetPropertiesSuccess(
    const std::string& service_path,
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

bool NetworkingPrivateGetManagedPropertiesFunction::RunAsync() {
  scoped_ptr<api::GetManagedProperties::Params> params =
      api::GetManagedProperties::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string service_path;
  if (!GetServicePathFromGuid(params->network_guid, &service_path, &error_))
    return false;

  std::string user_id_hash;
  if (!GetUserIdHash(browser_context(), &user_id_hash)) {
    // Disallow getManagedProperties from a non-primary user context to avoid
    // complexites with the policy code.
    NET_LOG_ERROR("getManagedProperties called from non primary user.",
                  browser_context()->GetPath().value());
    error_ = "Error.NonPrimaryUser";
    return false;
  }

  NetworkHandler::Get()->managed_network_configuration_handler()->
      GetManagedProperties(
          user_id_hash,
          service_path,
          base::Bind(&NetworkingPrivateGetManagedPropertiesFunction::Success,
                     this),
          base::Bind(&NetworkingPrivateGetManagedPropertiesFunction::Failure,
                     this));
  return true;
}

void NetworkingPrivateGetManagedPropertiesFunction::Success(
    const std::string& service_path,
    const base::DictionaryValue& dictionary) {
  SetResult(dictionary.DeepCopy());
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

bool NetworkingPrivateGetStateFunction::RunAsync() {
  scoped_ptr<api::GetState::Params> params =
      api::GetState::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string service_path;
  if (!GetServicePathFromGuid(params->network_guid, &service_path, &error_))
    return false;

  const NetworkState* network_state =
      NetworkHandler::Get()
          ->network_state_handler()
          ->GetNetworkStateFromServicePath(service_path,
                                           false /* configured_only */);
  if (!network_state) {
    error_ = "Error.NetworkUnavailable";
    return false;
  }

  scoped_ptr<base::DictionaryValue> network_properties =
      chromeos::network_util::TranslateNetworkStateToONC(network_state);

  SetResult(network_properties.release());
  SendResponse(true);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSetPropertiesFunction

NetworkingPrivateSetPropertiesFunction::
~NetworkingPrivateSetPropertiesFunction() {
}

bool NetworkingPrivateSetPropertiesFunction::RunAsync() {
  scoped_ptr<api::SetProperties::Params> params =
      api::SetProperties::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string service_path;
  if (!GetServicePathFromGuid(params->network_guid, &service_path, &error_))
    return false;

  scoped_ptr<base::DictionaryValue> properties_dict(
      params->properties.ToValue());

  NetworkHandler::Get()->managed_network_configuration_handler()->SetProperties(
      service_path,
      *properties_dict,
      base::Bind(&NetworkingPrivateSetPropertiesFunction::ResultCallback,
                 this),
      base::Bind(&NetworkingPrivateSetPropertiesFunction::ErrorCallback,
                 this));
  return true;
}

void NetworkingPrivateSetPropertiesFunction::ErrorCallback(
    const std::string& error_name,
    const scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
}

void NetworkingPrivateSetPropertiesFunction::ResultCallback() {
  SendResponse(true);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateCreateNetworkFunction

NetworkingPrivateCreateNetworkFunction::
~NetworkingPrivateCreateNetworkFunction() {
}

bool NetworkingPrivateCreateNetworkFunction::RunAsync() {
  scoped_ptr<api::CreateNetwork::Params> params =
      api::CreateNetwork::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string user_id_hash;
  if (!params->shared &&
      !GetUserIdHash(browser_context(), &user_id_hash)) {
    // Do not allow configuring a non-shared network from a non-primary user
    // context.
    NET_LOG_ERROR("createNetwork called from non primary user.",
                  browser_context()->GetPath().value());
    error_ = "Error.NonPrimaryUser";
    return false;
  }

  scoped_ptr<base::DictionaryValue> properties_dict(
      params->properties.ToValue());

  NetworkHandler::Get()->managed_network_configuration_handler()->
      CreateConfiguration(
          user_id_hash,
          *properties_dict,
          base::Bind(&NetworkingPrivateCreateNetworkFunction::ResultCallback,
                     this),
          base::Bind(&NetworkingPrivateCreateNetworkFunction::ErrorCallback,
                     this));
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
// NetworkingPrivateGetNetworksFunction

NetworkingPrivateGetNetworksFunction::
~NetworkingPrivateGetNetworksFunction() {
}

bool NetworkingPrivateGetNetworksFunction::RunAsync() {
  scoped_ptr<api::GetNetworks::Params> params =
      api::GetNetworks::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  NetworkTypePattern pattern = chromeos::onc::NetworkTypePatternFromOncType(
      api::ToString(params->filter.network_type));
  const bool configured_only =
      params->filter.configured ? *params->filter.configured : false;
  const bool visible_only =
      params->filter.visible ? *params->filter.visible : false;
  const int limit =
      params->filter.limit ? *params->filter.limit : kDefaultNetworkListLimit;
  scoped_ptr<base::ListValue> network_properties_list =
      chromeos::network_util::TranslateNetworkListToONC(
          pattern, configured_only, visible_only, limit, false /* debugging */);
  SetResult(network_properties_list.release());
  SendResponse(true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetVisibleNetworksFunction

NetworkingPrivateGetVisibleNetworksFunction::
~NetworkingPrivateGetVisibleNetworksFunction() {
}

bool NetworkingPrivateGetVisibleNetworksFunction::RunAsync() {
  scoped_ptr<api::GetVisibleNetworks::Params> params =
      api::GetVisibleNetworks::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  NetworkTypePattern pattern = chromeos::onc::NetworkTypePatternFromOncType(
      api::ToString(params->network_type));
  const bool configured_only = false;
  const bool visible_only = true;
  scoped_ptr<base::ListValue> network_properties_list =
      chromeos::network_util::TranslateNetworkListToONC(
          pattern, configured_only, visible_only, kDefaultNetworkListLimit,
          false /* debugging */);
  SetResult(network_properties_list.release());
  SendResponse(true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetEnabledNetworkTypesFunction

NetworkingPrivateGetEnabledNetworkTypesFunction::
~NetworkingPrivateGetEnabledNetworkTypesFunction() {
}

bool NetworkingPrivateGetEnabledNetworkTypesFunction::RunSync() {
  NetworkStateHandler* state_handler =
      NetworkHandler::Get()->network_state_handler();

  base::ListValue* network_list = new base::ListValue;

  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Ethernet()))
    network_list->AppendString(api::ToString(api::NETWORK_TYPE_ETHERNET));
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::WiFi()))
    network_list->AppendString(api::ToString(api::NETWORK_TYPE_WIFI));
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Wimax()))
    network_list->AppendString(api::ToString(api::NETWORK_TYPE_WIMAX));
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Cellular()))
    network_list->AppendString(api::ToString(api::NETWORK_TYPE_CELLULAR));

  SetResult(network_list);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateEnableNetworkTypeFunction

NetworkingPrivateEnableNetworkTypeFunction::
~NetworkingPrivateEnableNetworkTypeFunction() {
}

bool NetworkingPrivateEnableNetworkTypeFunction::RunSync() {
  scoped_ptr<api::EnableNetworkType::Params> params =
      api::EnableNetworkType::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  NetworkTypePattern pattern = chromeos::onc::NetworkTypePatternFromOncType(
      api::ToString(params->network_type));

  NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
      pattern, true, chromeos::network_handler::ErrorCallback());

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateDisableNetworkTypeFunction

NetworkingPrivateDisableNetworkTypeFunction::
~NetworkingPrivateDisableNetworkTypeFunction() {
}

bool NetworkingPrivateDisableNetworkTypeFunction::RunSync() {
  scoped_ptr<api::DisableNetworkType::Params> params =
      api::DisableNetworkType::Params::Create(*args_);

  NetworkTypePattern pattern = chromeos::onc::NetworkTypePatternFromOncType(
      api::ToString(params->network_type));

  NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
      pattern, false, chromeos::network_handler::ErrorCallback());

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateRequestNetworkScanFunction

NetworkingPrivateRequestNetworkScanFunction::
~NetworkingPrivateRequestNetworkScanFunction() {
}

bool NetworkingPrivateRequestNetworkScanFunction::RunSync() {
  NetworkHandler::Get()->network_state_handler()->RequestScan();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartConnectFunction

NetworkingPrivateStartConnectFunction::
  ~NetworkingPrivateStartConnectFunction() {
}

void  NetworkingPrivateStartConnectFunction::ConnectionStartSuccess() {
  SendResponse(true);
}

void NetworkingPrivateStartConnectFunction::ConnectionStartFailed(
    const std::string& error_name,
    const scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
}

bool NetworkingPrivateStartConnectFunction::RunAsync() {
  scoped_ptr<api::StartConnect::Params> params =
      api::StartConnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string service_path;
  if (!GetServicePathFromGuid(params->network_guid, &service_path, &error_))
    return false;

  const bool check_error_state = false;
  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      service_path,
      base::Bind(
          &NetworkingPrivateStartConnectFunction::ConnectionStartSuccess,
          this),
      base::Bind(
          &NetworkingPrivateStartConnectFunction::ConnectionStartFailed,
          this),
      check_error_state);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartDisconnectFunction

NetworkingPrivateStartDisconnectFunction::
  ~NetworkingPrivateStartDisconnectFunction() {
}

void  NetworkingPrivateStartDisconnectFunction::DisconnectionStartSuccess() {
  SendResponse(true);
}

void NetworkingPrivateStartDisconnectFunction::DisconnectionStartFailed(
    const std::string& error_name,
    const scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
}

bool NetworkingPrivateStartDisconnectFunction::RunAsync() {
  scoped_ptr<api::StartDisconnect::Params> params =
      api::StartDisconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string service_path;
  if (!GetServicePathFromGuid(params->network_guid, &service_path, &error_))
    return false;

  NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
      service_path,
      base::Bind(
          &NetworkingPrivateStartDisconnectFunction::DisconnectionStartSuccess,
          this),
      base::Bind(
          &NetworkingPrivateStartDisconnectFunction::DisconnectionStartFailed,
          this));
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyDestinationFunction

NetworkingPrivateVerifyDestinationFunction::
  ~NetworkingPrivateVerifyDestinationFunction() {
}

bool NetworkingPrivateVerifyDestinationFunction::RunAsync() {
  scoped_ptr<api::VerifyDestination::Params> params =
      api::VerifyDestination::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  ShillManagerClient::VerificationProperties verification_properties =
      ConvertVerificationProperties(params->properties);

  DBusThreadManager::Get()->GetShillManagerClient()->VerifyDestination(
      verification_properties,
      base::Bind(
          &NetworkingPrivateVerifyDestinationFunction::ResultCallback,
          this),
      base::Bind(
          &NetworkingPrivateVerifyDestinationFunction::ErrorCallback,
          this));
  return true;
}

void NetworkingPrivateVerifyDestinationFunction::ResultCallback(
    bool result) {
  results_ = api::VerifyDestination::Results::Create(result);
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

bool NetworkingPrivateVerifyAndEncryptCredentialsFunction::RunAsync() {
  scoped_ptr<api::VerifyAndEncryptCredentials::Params> params =
      api::VerifyAndEncryptCredentials::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string service_path;
  if (!GetServicePathFromGuid(params->network_guid, &service_path, &error_))
    return false;

  ShillManagerClient::VerificationProperties verification_properties =
      ConvertVerificationProperties(params->properties);

  ShillManagerClient* shill_manager_client =
      DBusThreadManager::Get()->GetShillManagerClient();
  shill_manager_client->VerifyAndEncryptCredentials(
      verification_properties,
      service_path,
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
  results_ = api::VerifyAndEncryptCredentials::Results::Create(result);
  SendResponse(true);
}

void NetworkingPrivateVerifyAndEncryptCredentialsFunction::ErrorCallback(
    const std::string& error_name, const std::string& error) {
  error_ = error_name;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyAndEncryptDataFunction

NetworkingPrivateVerifyAndEncryptDataFunction::
  ~NetworkingPrivateVerifyAndEncryptDataFunction() {
}

bool NetworkingPrivateVerifyAndEncryptDataFunction::RunAsync() {
  scoped_ptr<api::VerifyAndEncryptData::Params> params =
      api::VerifyAndEncryptData::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  ShillManagerClient::VerificationProperties verification_properties =
      ConvertVerificationProperties(params->properties);

  DBusThreadManager::Get()->GetShillManagerClient()->VerifyAndEncryptData(
      verification_properties,
      params->data,
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
  results_ = api::VerifyAndEncryptData::Results::Create(result);
  SendResponse(true);
}

void NetworkingPrivateVerifyAndEncryptDataFunction::ErrorCallback(
    const std::string& error_name,
    const std::string& error) {
  error_ = error_name;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSetWifiTDLSEnabledStateFunction

NetworkingPrivateSetWifiTDLSEnabledStateFunction::
  ~NetworkingPrivateSetWifiTDLSEnabledStateFunction() {
}

bool NetworkingPrivateSetWifiTDLSEnabledStateFunction::RunAsync() {
  scoped_ptr<api::SetWifiTDLSEnabledState::Params> params =
      api::SetWifiTDLSEnabledState::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string ip_or_mac_address = params->ip_or_mac_address;
  bool enable = params->enabled;

  NetworkHandler::Get()->network_device_handler()->
      SetWifiTDLSEnabled(
          ip_or_mac_address,
          enable,
          base::Bind(&NetworkingPrivateSetWifiTDLSEnabledStateFunction::Success,
                     this),
          base::Bind(&NetworkingPrivateSetWifiTDLSEnabledStateFunction::Failure,
                     this));

  return true;
}

void NetworkingPrivateSetWifiTDLSEnabledStateFunction::Success(
    const std::string& result) {
  results_ = api::SetWifiTDLSEnabledState::Results::Create(result);
  SendResponse(true);
}

void NetworkingPrivateSetWifiTDLSEnabledStateFunction::Failure(
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetWifiTDLSStatusFunction

NetworkingPrivateGetWifiTDLSStatusFunction::
  ~NetworkingPrivateGetWifiTDLSStatusFunction() {
}

bool NetworkingPrivateGetWifiTDLSStatusFunction::RunAsync() {
  scoped_ptr<api::GetWifiTDLSStatus::Params> params =
      api::GetWifiTDLSStatus::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string ip_or_mac_address = params->ip_or_mac_address;

  NetworkHandler::Get()->network_device_handler()->
      GetWifiTDLSStatus(
          ip_or_mac_address,
          base::Bind(&NetworkingPrivateGetWifiTDLSStatusFunction::Success,
                     this),
          base::Bind(&NetworkingPrivateGetWifiTDLSStatusFunction::Failure,
                     this));

  return true;
}

void NetworkingPrivateGetWifiTDLSStatusFunction::Success(
    const std::string& result) {
  results_ = api::GetWifiTDLSStatus::Results::Create(result);
  SendResponse(true);
}

void NetworkingPrivateGetWifiTDLSStatusFunction::Failure(
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  error_ = error_name;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetCaptivePortalStatusFunction

NetworkingPrivateGetCaptivePortalStatusFunction::
    ~NetworkingPrivateGetCaptivePortalStatusFunction() {}

bool NetworkingPrivateGetCaptivePortalStatusFunction::RunAsync() {
  scoped_ptr<api::GetCaptivePortalStatus::Params> params =
      api::GetCaptivePortalStatus::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  NetworkPortalDetector* detector = NetworkPortalDetector::Get();
  if (!detector) {
    error_ = "Error.NotReady";
    return false;
  }

  NetworkPortalDetector::CaptivePortalState state =
      detector->GetCaptivePortalState(params->network_guid);

  SetResult(new base::StringValue(
      NetworkPortalDetector::CaptivePortalStatusString(state.status)));
  SendResponse(true);
  return true;
}
