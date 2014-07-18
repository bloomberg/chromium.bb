// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_api.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_delegate.h"
#include "chrome/common/extensions/api/networking_private.h"
#include "components/onc/onc_constants.h"
#include "extensions/browser/extension_function_registry.h"

namespace {

const int kDefaultNetworkListLimit = 1000;

extensions::NetworkingPrivateDelegate* GetDelegate(
    content::BrowserContext* browser_context) {
  return extensions::NetworkingPrivateDelegate::GetForBrowserContext(
      browser_context);
}

}  // namespace

namespace private_api = extensions::api::networking_private;

namespace extensions {

namespace networking_private {

// static
const char kErrorInvalidNetworkGuid[] = "Error.InvalidNetworkGuid";
const char kErrorNetworkUnavailable[] = "Error.NetworkUnavailable";
const char kErrorEncryptionError[] = "Error.EncryptionError";
const char kErrorNotReady[] = "Error.NotReady";
const char kErrorNotSupported[] = "Error.NotSupported";

}  // namespace networking_private

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetPropertiesFunction

NetworkingPrivateGetPropertiesFunction::
    ~NetworkingPrivateGetPropertiesFunction() {
}

bool NetworkingPrivateGetPropertiesFunction::RunAsync() {
  scoped_ptr<private_api::GetProperties::Params> params =
      private_api::GetProperties::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())->GetProperties(
      params->network_guid,
      base::Bind(&NetworkingPrivateGetPropertiesFunction::Success, this),
      base::Bind(&NetworkingPrivateGetPropertiesFunction::Failure, this));
  return true;
}

void NetworkingPrivateGetPropertiesFunction::Success(
    scoped_ptr<base::DictionaryValue> result) {
  SetResult(result.release());
  SendResponse(true);
}

void NetworkingPrivateGetPropertiesFunction::Failure(const std::string& error) {
  error_ = error;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetManagedPropertiesFunction

NetworkingPrivateGetManagedPropertiesFunction::
    ~NetworkingPrivateGetManagedPropertiesFunction() {
}

bool NetworkingPrivateGetManagedPropertiesFunction::RunAsync() {
  scoped_ptr<private_api::GetManagedProperties::Params> params =
      private_api::GetManagedProperties::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())->GetManagedProperties(
      params->network_guid,
      base::Bind(&NetworkingPrivateGetManagedPropertiesFunction::Success, this),
      base::Bind(&NetworkingPrivateGetManagedPropertiesFunction::Failure,
                 this));
  return true;
}

void NetworkingPrivateGetManagedPropertiesFunction::Success(
    scoped_ptr<base::DictionaryValue> result) {
  SetResult(result.release());
  SendResponse(true);
}

void NetworkingPrivateGetManagedPropertiesFunction::Failure(
    const std::string& error) {
  error_ = error;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetStateFunction

NetworkingPrivateGetStateFunction::~NetworkingPrivateGetStateFunction() {
}

bool NetworkingPrivateGetStateFunction::RunAsync() {
  scoped_ptr<private_api::GetState::Params> params =
      private_api::GetState::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())->GetState(
      params->network_guid,
      base::Bind(&NetworkingPrivateGetStateFunction::Success, this),
      base::Bind(&NetworkingPrivateGetStateFunction::Failure, this));
  return true;
}

void NetworkingPrivateGetStateFunction::Success(
    scoped_ptr<base::DictionaryValue> result) {
  SetResult(result.release());
  SendResponse(true);
}

void NetworkingPrivateGetStateFunction::Failure(const std::string& error) {
  error_ = error;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSetPropertiesFunction

NetworkingPrivateSetPropertiesFunction::
    ~NetworkingPrivateSetPropertiesFunction() {
}

bool NetworkingPrivateSetPropertiesFunction::RunAsync() {
  scoped_ptr<private_api::SetProperties::Params> params =
      private_api::SetProperties::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_ptr<base::DictionaryValue> properties_dict(
      params->properties.ToValue());

  GetDelegate(browser_context())->SetProperties(
      params->network_guid,
      properties_dict.Pass(),
      base::Bind(&NetworkingPrivateSetPropertiesFunction::Success, this),
      base::Bind(&NetworkingPrivateSetPropertiesFunction::Failure, this));
  return true;
}

void NetworkingPrivateSetPropertiesFunction::Success() {
  SendResponse(true);
}

void NetworkingPrivateSetPropertiesFunction::Failure(const std::string& error) {
  error_ = error;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateCreateNetworkFunction

NetworkingPrivateCreateNetworkFunction::
    ~NetworkingPrivateCreateNetworkFunction() {
}

bool NetworkingPrivateCreateNetworkFunction::RunAsync() {
  scoped_ptr<private_api::CreateNetwork::Params> params =
      private_api::CreateNetwork::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_ptr<base::DictionaryValue> properties_dict(
      params->properties.ToValue());

  GetDelegate(browser_context())->CreateNetwork(
      params->shared,
      properties_dict.Pass(),
      base::Bind(&NetworkingPrivateCreateNetworkFunction::Success, this),
      base::Bind(&NetworkingPrivateCreateNetworkFunction::Failure, this));
  return true;
}

void NetworkingPrivateCreateNetworkFunction::Success(const std::string& guid) {
  results_ = private_api::CreateNetwork::Results::Create(guid);
  SendResponse(true);
}

void NetworkingPrivateCreateNetworkFunction::Failure(const std::string& error) {
  error_ = error;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetNetworksFunction

NetworkingPrivateGetNetworksFunction::~NetworkingPrivateGetNetworksFunction() {
}

bool NetworkingPrivateGetNetworksFunction::RunAsync() {
  scoped_ptr<private_api::GetNetworks::Params> params =
      private_api::GetNetworks::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string network_type = private_api::ToString(params->filter.network_type);
  const bool configured_only =
      params->filter.configured ? *params->filter.configured : false;
  const bool visible_only =
      params->filter.visible ? *params->filter.visible : false;
  const int limit =
      params->filter.limit ? *params->filter.limit : kDefaultNetworkListLimit;

  GetDelegate(browser_context())->GetNetworks(
      network_type,
      configured_only,
      visible_only,
      limit,
      base::Bind(&NetworkingPrivateGetNetworksFunction::Success, this),
      base::Bind(&NetworkingPrivateGetNetworksFunction::Failure, this));
  return true;
}

void NetworkingPrivateGetNetworksFunction::Success(
    scoped_ptr<base::ListValue> network_list) {
  SetResult(network_list.release());
  SendResponse(true);
}

void NetworkingPrivateGetNetworksFunction::Failure(const std::string& error) {
  error_ = error;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetVisibleNetworksFunction

NetworkingPrivateGetVisibleNetworksFunction::
    ~NetworkingPrivateGetVisibleNetworksFunction() {
}

bool NetworkingPrivateGetVisibleNetworksFunction::RunAsync() {
  scoped_ptr<private_api::GetVisibleNetworks::Params> params =
      private_api::GetVisibleNetworks::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string network_type = private_api::ToString(params->network_type);
  const bool configured_only = false;
  const bool visible_only = true;

  GetDelegate(browser_context())->GetNetworks(
      network_type,
      configured_only,
      visible_only,
      kDefaultNetworkListLimit,
      base::Bind(&NetworkingPrivateGetVisibleNetworksFunction::Success, this),
      base::Bind(&NetworkingPrivateGetVisibleNetworksFunction::Failure, this));
  return true;
}

void NetworkingPrivateGetVisibleNetworksFunction::Success(
    scoped_ptr<base::ListValue> network_properties_list) {
  SetResult(network_properties_list.release());
  SendResponse(true);
}

void NetworkingPrivateGetVisibleNetworksFunction::Failure(
    const std::string& error) {
  error_ = error;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetEnabledNetworkTypesFunction

NetworkingPrivateGetEnabledNetworkTypesFunction::
    ~NetworkingPrivateGetEnabledNetworkTypesFunction() {
}

bool NetworkingPrivateGetEnabledNetworkTypesFunction::RunSync() {
  scoped_ptr<base::ListValue> enabled_networks_onc_types(
      GetDelegate(browser_context())->GetEnabledNetworkTypes());
  if (!enabled_networks_onc_types) {
    error_ = networking_private::kErrorNotSupported;
    return false;
  }
  scoped_ptr<base::ListValue> enabled_networks_list;
  for (base::ListValue::iterator iter = enabled_networks_onc_types->begin();
       iter != enabled_networks_onc_types->end(); ++iter) {
    std::string type;
    if (!(*iter)->GetAsString(&type))
      NOTREACHED();
    if (type == ::onc::network_type::kEthernet) {
      enabled_networks_list->AppendString(api::networking_private::ToString(
          api::networking_private::NETWORK_TYPE_ETHERNET));
    } else if (type == ::onc::network_type::kWiFi) {
      enabled_networks_list->AppendString(api::networking_private::ToString(
          api::networking_private::NETWORK_TYPE_WIFI));
    } else if (type == ::onc::network_type::kWimax) {
      enabled_networks_list->AppendString(api::networking_private::ToString(
          api::networking_private::NETWORK_TYPE_WIMAX));
    } else if (type == ::onc::network_type::kCellular) {
      enabled_networks_list->AppendString(api::networking_private::ToString(
          api::networking_private::NETWORK_TYPE_CELLULAR));
    } else {
      LOG(ERROR) << "networkingPrivate: Unexpected type: " << type;
    }
  }
  SetResult(enabled_networks_list.release());
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateEnableNetworkTypeFunction

NetworkingPrivateEnableNetworkTypeFunction::
    ~NetworkingPrivateEnableNetworkTypeFunction() {
}

bool NetworkingPrivateEnableNetworkTypeFunction::RunSync() {
  scoped_ptr<private_api::EnableNetworkType::Params> params =
      private_api::EnableNetworkType::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  return GetDelegate(browser_context())->EnableNetworkType(
      private_api::ToString(params->network_type));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateDisableNetworkTypeFunction

NetworkingPrivateDisableNetworkTypeFunction::
    ~NetworkingPrivateDisableNetworkTypeFunction() {
}

bool NetworkingPrivateDisableNetworkTypeFunction::RunSync() {
  scoped_ptr<private_api::DisableNetworkType::Params> params =
      private_api::DisableNetworkType::Params::Create(*args_);

  return GetDelegate(browser_context())->DisableNetworkType(
      private_api::ToString(params->network_type));
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateRequestNetworkScanFunction

NetworkingPrivateRequestNetworkScanFunction::
    ~NetworkingPrivateRequestNetworkScanFunction() {
}

bool NetworkingPrivateRequestNetworkScanFunction::RunSync() {
  return GetDelegate(browser_context())->RequestScan();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartConnectFunction

NetworkingPrivateStartConnectFunction::
    ~NetworkingPrivateStartConnectFunction() {
}

bool NetworkingPrivateStartConnectFunction::RunAsync() {
  scoped_ptr<private_api::StartConnect::Params> params =
      private_api::StartConnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())->StartConnect(
      params->network_guid,
      base::Bind(&NetworkingPrivateStartConnectFunction::Success, this),
      base::Bind(&NetworkingPrivateStartConnectFunction::Failure, this));
  return true;
}

void NetworkingPrivateStartConnectFunction::Success() {
  SendResponse(true);
}

void NetworkingPrivateStartConnectFunction::Failure(const std::string& error) {
  error_ = error;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateStartDisconnectFunction

NetworkingPrivateStartDisconnectFunction::
    ~NetworkingPrivateStartDisconnectFunction() {
}

bool NetworkingPrivateStartDisconnectFunction::RunAsync() {
  scoped_ptr<private_api::StartDisconnect::Params> params =
      private_api::StartDisconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())->StartDisconnect(
      params->network_guid,
      base::Bind(&NetworkingPrivateStartDisconnectFunction::Success, this),
      base::Bind(&NetworkingPrivateStartDisconnectFunction::Failure, this));
  return true;
}

void NetworkingPrivateStartDisconnectFunction::Success() {
  SendResponse(true);
}

void NetworkingPrivateStartDisconnectFunction::Failure(
    const std::string& error) {
  error_ = error;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyDestinationFunction

NetworkingPrivateVerifyDestinationFunction::
    ~NetworkingPrivateVerifyDestinationFunction() {
}

bool NetworkingPrivateVerifyDestinationFunction::RunAsync() {
  scoped_ptr<private_api::VerifyDestination::Params> params =
      private_api::VerifyDestination::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())->VerifyDestination(
      params->properties,
      base::Bind(&NetworkingPrivateVerifyDestinationFunction::Success, this),
      base::Bind(&NetworkingPrivateVerifyDestinationFunction::Failure, this));
  return true;
}

void NetworkingPrivateVerifyDestinationFunction::Success(bool result) {
  results_ = private_api::VerifyDestination::Results::Create(result);
  SendResponse(true);
}

void NetworkingPrivateVerifyDestinationFunction::Failure(
    const std::string& error) {
  error_ = error;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyAndEncryptCredentialsFunction

NetworkingPrivateVerifyAndEncryptCredentialsFunction::
    ~NetworkingPrivateVerifyAndEncryptCredentialsFunction() {
}

bool NetworkingPrivateVerifyAndEncryptCredentialsFunction::RunAsync() {
  scoped_ptr<private_api::VerifyAndEncryptCredentials::Params> params =
      private_api::VerifyAndEncryptCredentials::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())->VerifyAndEncryptCredentials(
      params->network_guid,
      params->properties,
      base::Bind(&NetworkingPrivateVerifyAndEncryptCredentialsFunction::Success,
                 this),
      base::Bind(&NetworkingPrivateVerifyAndEncryptCredentialsFunction::Failure,
                 this));
  return true;
}

void NetworkingPrivateVerifyAndEncryptCredentialsFunction::Success(
    const std::string& result) {
  results_ = private_api::VerifyAndEncryptCredentials::Results::Create(result);
  SendResponse(true);
}

void NetworkingPrivateVerifyAndEncryptCredentialsFunction::Failure(
    const std::string& error) {
  error_ = error;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyAndEncryptDataFunction

NetworkingPrivateVerifyAndEncryptDataFunction::
    ~NetworkingPrivateVerifyAndEncryptDataFunction() {
}

bool NetworkingPrivateVerifyAndEncryptDataFunction::RunAsync() {
  scoped_ptr<private_api::VerifyAndEncryptData::Params> params =
      private_api::VerifyAndEncryptData::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())->VerifyAndEncryptData(
      params->properties,
      params->data,
      base::Bind(&NetworkingPrivateVerifyAndEncryptDataFunction::Success, this),
      base::Bind(&NetworkingPrivateVerifyAndEncryptDataFunction::Failure,
                 this));
  return true;
}

void NetworkingPrivateVerifyAndEncryptDataFunction::Success(
    const std::string& result) {
  results_ = private_api::VerifyAndEncryptData::Results::Create(result);
  SendResponse(true);
}

void NetworkingPrivateVerifyAndEncryptDataFunction::Failure(
    const std::string& error) {
  error_ = error;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateSetWifiTDLSEnabledStateFunction

NetworkingPrivateSetWifiTDLSEnabledStateFunction::
    ~NetworkingPrivateSetWifiTDLSEnabledStateFunction() {
}

bool NetworkingPrivateSetWifiTDLSEnabledStateFunction::RunAsync() {
  scoped_ptr<private_api::SetWifiTDLSEnabledState::Params> params =
      private_api::SetWifiTDLSEnabledState::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())->SetWifiTDLSEnabledState(
      params->ip_or_mac_address,
      params->enabled,
      base::Bind(&NetworkingPrivateSetWifiTDLSEnabledStateFunction::Success,
                 this),
      base::Bind(&NetworkingPrivateSetWifiTDLSEnabledStateFunction::Failure,
                 this));

  return true;
}

void NetworkingPrivateSetWifiTDLSEnabledStateFunction::Success(
    const std::string& result) {
  results_ = private_api::SetWifiTDLSEnabledState::Results::Create(result);
  SendResponse(true);
}

void NetworkingPrivateSetWifiTDLSEnabledStateFunction::Failure(
    const std::string& error) {
  error_ = error;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetWifiTDLSStatusFunction

NetworkingPrivateGetWifiTDLSStatusFunction::
    ~NetworkingPrivateGetWifiTDLSStatusFunction() {
}

bool NetworkingPrivateGetWifiTDLSStatusFunction::RunAsync() {
  scoped_ptr<private_api::GetWifiTDLSStatus::Params> params =
      private_api::GetWifiTDLSStatus::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())->GetWifiTDLSStatus(
      params->ip_or_mac_address,
      base::Bind(&NetworkingPrivateGetWifiTDLSStatusFunction::Success, this),
      base::Bind(&NetworkingPrivateGetWifiTDLSStatusFunction::Failure, this));

  return true;
}

void NetworkingPrivateGetWifiTDLSStatusFunction::Success(
    const std::string& result) {
  results_ = private_api::GetWifiTDLSStatus::Results::Create(result);
  SendResponse(true);
}

void NetworkingPrivateGetWifiTDLSStatusFunction::Failure(
    const std::string& error) {
  error_ = error;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateGetCaptivePortalStatusFunction

NetworkingPrivateGetCaptivePortalStatusFunction::
    ~NetworkingPrivateGetCaptivePortalStatusFunction() {
}

bool NetworkingPrivateGetCaptivePortalStatusFunction::RunAsync() {
  scoped_ptr<private_api::GetCaptivePortalStatus::Params> params =
      private_api::GetCaptivePortalStatus::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  GetDelegate(browser_context())->GetCaptivePortalStatus(
      params->network_guid,
      base::Bind(&NetworkingPrivateGetCaptivePortalStatusFunction::Success,
                 this),
      base::Bind(&NetworkingPrivateGetCaptivePortalStatusFunction::Failure,
                 this));
  return true;
}

void NetworkingPrivateGetCaptivePortalStatusFunction::Success(
    const std::string& result) {
  results_ = private_api::GetCaptivePortalStatus::Results::Create(
      private_api::ParseCaptivePortalStatus(result));
  SendResponse(true);
}

void NetworkingPrivateGetCaptivePortalStatusFunction::Failure(
    const std::string& error) {
  error_ = error;
  SendResponse(false);
}

}  // namespace extensions
