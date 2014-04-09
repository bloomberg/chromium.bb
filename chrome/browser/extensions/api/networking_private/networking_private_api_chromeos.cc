// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_api.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/networking_private.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/shill_property_util.h"
#include "components/onc/onc_constants.h"
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

std::string GetUserIdHash(Profile* profile) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kMultiProfiles)) {
    return g_browser_process->platform_part()->
        profile_helper()->GetUserIdHashFromProfile(profile);
  } else {
    return g_browser_process->platform_part()->
        profile_helper()->active_user_id_hash();
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetPropertiesFunction

NetworkingPrivateGetPropertiesFunction::
  ~NetworkingPrivateGetPropertiesFunction() {
}

bool NetworkingPrivateGetPropertiesFunction::RunImpl() {
  scoped_ptr<api::GetProperties::Params> params =
      api::GetProperties::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  NetworkHandler::Get()->managed_network_configuration_handler()->GetProperties(
      params->network_guid,  // service path
      base::Bind(&NetworkingPrivateGetPropertiesFunction::GetPropertiesSuccess,
                 this),
      base::Bind(&NetworkingPrivateGetPropertiesFunction::GetPropertiesFailed,
                 this));
  return true;
}

void NetworkingPrivateGetPropertiesFunction::GetPropertiesSuccess(
    const std::string& service_path,
    const base::DictionaryValue& dictionary) {
  base::DictionaryValue* network_properties = dictionary.DeepCopy();
  network_properties->SetStringWithoutPathExpansion(onc::network_config::kGUID,
                                                    service_path);
  SetResult(network_properties);
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

  std::string user_id_hash;
  GetUserIdHash(GetProfile());
  NetworkHandler::Get()->managed_network_configuration_handler()->
      GetManagedProperties(
          user_id_hash,
          params->network_guid,  // service path
          base::Bind(&NetworkingPrivateGetManagedPropertiesFunction::Success,
                     this),
          base::Bind(&NetworkingPrivateGetManagedPropertiesFunction::Failure,
                     this));
  return true;
}

void NetworkingPrivateGetManagedPropertiesFunction::Success(
    const std::string& service_path,
    const base::DictionaryValue& dictionary) {
  base::DictionaryValue* network_properties = dictionary.DeepCopy();
  network_properties->SetStringWithoutPathExpansion(onc::network_config::kGUID,
                                                    service_path);
  SetResult(network_properties);
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
  // The |network_guid| parameter is storing the service path.
  std::string service_path = params->network_guid;

  const NetworkState* state = NetworkHandler::Get()->network_state_handler()->
      GetNetworkState(service_path);
  if (!state) {
    error_ = "Error.InvalidParameter";
    return false;
  }

  scoped_ptr<base::DictionaryValue> result_dict(new base::DictionaryValue);
  state->GetProperties(result_dict.get());
  scoped_ptr<base::DictionaryValue> onc_network_part =
      chromeos::onc::TranslateShillServiceToONCPart(*result_dict,
          &chromeos::onc::kNetworkWithStateSignature);
  SetResult(onc_network_part.release());
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

  NetworkHandler::Get()->managed_network_configuration_handler()->SetProperties(
      params->network_guid,  // service path
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

bool NetworkingPrivateCreateNetworkFunction::RunImpl() {
  scoped_ptr<api::CreateNetwork::Params> params =
      api::CreateNetwork::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string user_id_hash;
  if (!params->shared)
    user_id_hash = GetUserIdHash(GetProfile());

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
// NetworkingPrivateGetVisibleNetworksFunction

NetworkingPrivateGetVisibleNetworksFunction::
~NetworkingPrivateGetVisibleNetworksFunction() {
}

bool NetworkingPrivateGetVisibleNetworksFunction::RunImpl() {
  scoped_ptr<api::GetVisibleNetworks::Params> params =
      api::GetVisibleNetworks::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string type_filter =
      api::GetVisibleNetworks::Params::ToString(params->type);

  NetworkStateHandler::NetworkStateList network_states;
  NetworkHandler::Get()->network_state_handler()->GetNetworkList(
      &network_states);

  base::ListValue* network_properties_list = new base::ListValue;
  for (NetworkStateHandler::NetworkStateList::iterator it =
           network_states.begin();
       it != network_states.end(); ++it) {
    const std::string& service_path = (*it)->path();
    base::DictionaryValue shill_dictionary;
    (*it)->GetProperties(&shill_dictionary);

    scoped_ptr<base::DictionaryValue> onc_network_part =
        chromeos::onc::TranslateShillServiceToONCPart(shill_dictionary,
            &chromeos::onc::kNetworkWithStateSignature);

    std::string onc_type;
    onc_network_part->GetStringWithoutPathExpansion(onc::network_config::kType,
                                                    &onc_type);
    if (type_filter == onc::network_type::kAllTypes ||
        onc_type == type_filter) {
      onc_network_part->SetStringWithoutPathExpansion(
          onc::network_config::kGUID,
          service_path);
      network_properties_list->Append(onc_network_part.release());
    }
  }

  SetResult(network_properties_list);
  SendResponse(true);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetEnabledNetworkTypesFunction

NetworkingPrivateGetEnabledNetworkTypesFunction::
~NetworkingPrivateGetEnabledNetworkTypesFunction() {
}

bool NetworkingPrivateGetEnabledNetworkTypesFunction::RunImpl() {
  NetworkStateHandler* state_handler =
      NetworkHandler::Get()->network_state_handler();

  base::ListValue* network_list = new base::ListValue;

  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Ethernet()))
    network_list->AppendString("Ethernet");
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::WiFi()))
    network_list->AppendString("WiFi");
  if (state_handler->IsTechnologyEnabled(NetworkTypePattern::Cellular()))
    network_list->AppendString("Cellular");

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
  NetworkStateHandler* state_handler =
      NetworkHandler::Get()->network_state_handler();

  switch (params->network_type) {
    case api::NETWORK_TYPE_ETHERNET:
      state_handler->SetTechnologyEnabled(
          NetworkTypePattern::Ethernet(), true,
          chromeos::network_handler::ErrorCallback());
      break;

    case api::NETWORK_TYPE_WIFI:
      state_handler->SetTechnologyEnabled(
          NetworkTypePattern::WiFi(), true,
          chromeos::network_handler::ErrorCallback());
      break;

    case api::NETWORK_TYPE_CELLULAR:
      state_handler->SetTechnologyEnabled(
          NetworkTypePattern::Cellular(), true,
          chromeos::network_handler::ErrorCallback());
      break;

    default:
      break;
  }
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
  NetworkStateHandler* state_handler =
      NetworkHandler::Get()->network_state_handler();

  switch (params->network_type) {
    case api::NETWORK_TYPE_ETHERNET:
      state_handler->SetTechnologyEnabled(
          NetworkTypePattern::Ethernet(), false,
          chromeos::network_handler::ErrorCallback());
      break;

    case api::NETWORK_TYPE_WIFI:
      state_handler->SetTechnologyEnabled(
          NetworkTypePattern::WiFi(), false,
          chromeos::network_handler::ErrorCallback());
      break;

    case api::NETWORK_TYPE_CELLULAR:
      state_handler->SetTechnologyEnabled(
          NetworkTypePattern::Cellular(), false,
          chromeos::network_handler::ErrorCallback());
      break;

    default:
      break;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateRequestNetworkScanFunction

NetworkingPrivateRequestNetworkScanFunction::
~NetworkingPrivateRequestNetworkScanFunction() {
}

bool NetworkingPrivateRequestNetworkScanFunction::RunImpl() {
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

bool NetworkingPrivateStartConnectFunction::RunImpl() {
  scoped_ptr<api::StartConnect::Params> params =
      api::StartConnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  const bool check_error_state = false;
  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      params->network_guid,  // service path
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

bool NetworkingPrivateStartDisconnectFunction::RunImpl() {
  scoped_ptr<api::StartDisconnect::Params> params =
      api::StartDisconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  NetworkHandler::Get()->network_connection_handler()->DisconnectNetwork(
      params->network_guid,  // service path
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

bool NetworkingPrivateVerifyDestinationFunction::RunImpl() {
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

bool NetworkingPrivateVerifyAndEncryptCredentialsFunction::RunImpl() {
  scoped_ptr<api::VerifyAndEncryptCredentials::Params> params =
      api::VerifyAndEncryptCredentials::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  ShillManagerClient* shill_manager_client =
      DBusThreadManager::Get()->GetShillManagerClient();

  ShillManagerClient::VerificationProperties verification_properties =
      ConvertVerificationProperties(params->properties);

  shill_manager_client->VerifyAndEncryptCredentials(
      verification_properties,
      params->guid,
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

bool NetworkingPrivateVerifyAndEncryptDataFunction::RunImpl() {
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

bool NetworkingPrivateSetWifiTDLSEnabledStateFunction::RunImpl() {
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

bool NetworkingPrivateGetWifiTDLSStatusFunction::RunImpl() {
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

bool NetworkingPrivateGetCaptivePortalStatusFunction::RunImpl() {
  scoped_ptr<api::GetCaptivePortalStatus::Params> params =
      api::GetCaptivePortalStatus::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  NetworkPortalDetector* detector = NetworkPortalDetector::Get();
  if (!detector) {
    error_ = "Error.NotReady";
    return false;
  }

  NetworkPortalDetector::CaptivePortalState state =
      detector->GetCaptivePortalState(params->network_path);

  SetResult(new base::StringValue(
      NetworkPortalDetector::CaptivePortalStatusString(state.status)));
  SendResponse(true);
  return true;
}
