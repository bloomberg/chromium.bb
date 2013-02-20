// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/networking_private_api.h"

#include "base/bind_helpers.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/common/extensions/api/networking_private.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translation_tables.h"
#include "chromeos/network/onc/onc_translator.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using namespace chromeos;
namespace api = extensions::api::networking_private;

namespace {

// An error returned when no valid services were found.
const char kInvalidResponseError[] = "Error.invalidResponse";

// This creates a new ONC dictionary that only contains the information we're
// interested in passing on to JavaScript.
scoped_ptr<api::NetworkProperties> CreateFilteredResult(
    const base::DictionaryValue& properties) {
  scoped_ptr<base::DictionaryValue> onc_properties(
      onc::TranslateShillServiceToONCPart(
          properties,
          &onc::kNetworkConfigurationSignature));

  // Now we filter it so we only include properties that we care about for this
  // interface.
  static const char* const desired_fields[] = {
      onc::network_config::kWiFi,
      onc::network_config::kName,
      onc::network_config::kGUID,
      onc::network_config::kType,
      onc::network_config::kConnectionState,
  };

  scoped_ptr<api::NetworkProperties> filtered_result(
      new api::NetworkProperties);
  for (size_t i = 0; i < arraysize(desired_fields); ++i) {
    base::Value* value;
    if (onc_properties->Get(desired_fields[i], &value))
      filtered_result->additional_properties.Set(desired_fields[i],
                                                 value->DeepCopy());
  }

  return filtered_result.Pass();
}

class ResultList : public base::RefCounted<ResultList> {
 public:
  typedef base::Callback<void(const std::string& error,
                              scoped_ptr<base::ListValue>)> ResultCallback;

  ResultList(const std::string& type, const ResultCallback& callback)
    : callback_(callback) {
    DBusThreadManager::Get()->GetShillManagerClient()->GetProperties(
        base::Bind(&ResultList::ManagerPropertiesCallback, this, type));
  }

  scoped_ptr<base::ListValue> GetResults() {
    return api::GetVisibleNetworks::Results::Create(list_);
  }

 private:
  friend class base::RefCounted<ResultList>;

  ~ResultList() {
    callback_.Run(std::string(), GetResults());
  }

  void Append(api::NetworkProperties* value) {
    list_.push_back(linked_ptr<api::NetworkProperties>(value));
  }

  // Receives the result of a call to GetProperties on the Shill Manager API.
  void ManagerPropertiesCallback(const std::string& network_type,
                                 chromeos::DBusMethodCallStatus call_status,
                                 const base::DictionaryValue& result);

  // Receives the result of a call to GetProperties on the Shill Service API.
  void ServicePropertiesCallback(const std::string& service_path,
                                 const std::string& network_type,
                                 chromeos::DBusMethodCallStatus call_status,
                                 const base::DictionaryValue& result);

  std::vector<linked_ptr<api::NetworkProperties> > list_;
  ResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ResultList);
};

// For each of the available services, fire off a request for its properties.
void ResultList::ManagerPropertiesCallback(
    const std::string& network_type,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& result) {
  const base::ListValue* available_services;
  if (!result.GetList(flimflam::kServicesProperty, &available_services)) {
    LOG(ERROR)
        << "ShillManagerClient::GetProperties returned malformed service list.";
    callback_.Run(kInvalidResponseError, make_scoped_ptr(new base::ListValue));
    return;
  }
  // If there just are no services, return an empty list.
  if (available_services->GetSize() == 0) {
    callback_.Run(std::string(), make_scoped_ptr(new base::ListValue));
    return;
  }
  for (base::ListValue::const_iterator iter = available_services->begin();
       iter != available_services->end(); ++iter) {
    std::string service_path;
    if (!(*iter)->GetAsString(&service_path)) {
      LOG(ERROR)
          << "ShillManagerClient::GetProperties returned malformed service.";
      continue;
    }

    DBusThreadManager::Get()->GetShillServiceClient()->GetProperties(
        dbus::ObjectPath(service_path),
        base::Bind(
            &ResultList::ServicePropertiesCallback,
            this,
            service_path,
            network_type));
  }
}

void ResultList::ServicePropertiesCallback(
    const std::string& service_path,
    const std::string& network_type,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& result) {
  if (call_status == DBUS_METHOD_CALL_SUCCESS) {
    scoped_ptr<api::NetworkProperties> filtered_result(
        CreateFilteredResult(result));

    std::string onc_type;
    if (filtered_result->additional_properties.GetString(
        onc::network_config::kType, &onc_type) &&
        (onc_type == network_type ||
         network_type == onc::network_type::kAllTypes)) {
      // TODO(gspencer): For now the "GUID" we send back is going to look
      // remarkably like the service path. Once this code starts using the
      // NetworkStateHandler instead of Shill directly, we should remove
      // this line so that we're sending back the actual GUID. The
      // JavaScript shouldn't care: this ID is opaque to it, and it
      // shouldn't store it anywhere.
      filtered_result->additional_properties.SetString(
          onc::network_config::kGUID, service_path);

      Append(filtered_result.release());
    }
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

  // TODO(gspencer): Currently we're using the service path as the
  // |network_guid|. Eventually this should be using the real GUID.
  DBusThreadManager::Get()->GetShillServiceClient()->GetProperties(
      dbus::ObjectPath(params->network_guid),
      base::Bind(&NetworkingPrivateGetPropertiesFunction::ResultCallback,
                 this));
  return true;
}

void NetworkingPrivateGetPropertiesFunction::ResultCallback(
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& result) {
  scoped_ptr<api::NetworkProperties> filtered_result(
      CreateFilteredResult(result));
  results_ = api::GetProperties::Results::Create(*filtered_result);
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

  scoped_refptr<ResultList> result_list(new ResultList(
      api::GetVisibleNetworks::Params::ToString(params->type),
      base::Bind(
          &NetworkingPrivateGetVisibleNetworksFunction::SendResultCallback,
          this)));
  return true;
}

void NetworkingPrivateGetVisibleNetworksFunction::SendResultCallback(
    const std::string& error,
    scoped_ptr<base::ListValue> result_list) {
  if (!error.empty()) {
    error_ = error;
    SendResponse(false);
  } else {
    results_.reset(result_list.release());
    SendResponse(true);
  }
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
    const std::string& error_message) {
  error_ = error_name;
  SendResponse(false);
}

bool NetworkingPrivateStartConnectFunction::RunImpl() {
  scoped_ptr<api::StartConnect::Params> params =
      api::StartConnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // TODO(gspencer): For now, the "GUID" we receive from the JavaScript is going
  // to be the service path.  Fix this so it actually looks up the service path
  // from the GUID once we're using the NetworkStateHandler.
  std::string service_path = params->network_guid;

  DBusThreadManager::Get()->GetShillServiceClient()->Connect(
      dbus::ObjectPath(service_path),
      base::Bind(
          &NetworkingPrivateStartConnectFunction::ConnectionStartSuccess,
          this),
      base::Bind(&NetworkingPrivateStartConnectFunction::ConnectionStartFailed,
                 this));
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
    const std::string& error_message) {
  error_ = error_name;
  SendResponse(false);
}

bool NetworkingPrivateStartDisconnectFunction::RunImpl() {
  scoped_ptr<api::StartDisconnect::Params> params =
      api::StartDisconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // TODO(gspencer): Currently the |network_guid| parameter is storing the
  // service path.  Convert to using the actual GUID when we start using
  // the NetworkStateHandler.
  DBusThreadManager::Get()->GetShillServiceClient()->Disconnect(
      dbus::ObjectPath(params->network_guid),
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

  DBusThreadManager::Get()->GetShillManagerClient()->VerifyDestination(
      params->properties.certificate,
      params->properties.public_key,
      params->properties.nonce,
      params->properties.signed_data,
      params->properties.device_serial,
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
// NetworkingPrivateVerifyAndSignCredentialsFunction

NetworkingPrivateVerifyAndSignCredentialsFunction::
  ~NetworkingPrivateVerifyAndSignCredentialsFunction() {
}

bool NetworkingPrivateVerifyAndSignCredentialsFunction::RunImpl() {
  scoped_ptr<api::VerifyAndSignCredentials::Params> params =
      api::VerifyAndSignCredentials::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  DBusThreadManager::Get()->GetShillManagerClient()->VerifyAndSignCredentials(
      params->properties.certificate,
      params->properties.public_key,
      params->properties.nonce,
      params->properties.signed_data,
      params->properties.device_serial,
      params->guid,
      base::Bind(
          &NetworkingPrivateVerifyAndSignCredentialsFunction::ResultCallback,
          this),
      base::Bind(
          &NetworkingPrivateVerifyAndSignCredentialsFunction::ErrorCallback,
          this));
  return true;
}

void NetworkingPrivateVerifyAndSignCredentialsFunction::ResultCallback(
    const std::string& result) {
  results_ = api::VerifyAndSignCredentials::Results::Create(result);
  SendResponse(true);
}

void NetworkingPrivateVerifyAndSignCredentialsFunction::ErrorCallback(
    const std::string& error_name, const std::string& error) {
  error_ = error_name;
  SendResponse(false);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkingPrivateVerifyAndSignDataFunction

NetworkingPrivateVerifyAndSignDataFunction::
  ~NetworkingPrivateVerifyAndSignDataFunction() {
}

bool NetworkingPrivateVerifyAndSignDataFunction::RunImpl() {
  scoped_ptr<api::VerifyAndSignData::Params> params =
      api::VerifyAndSignData::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  DBusThreadManager::Get()->GetShillManagerClient()->VerifyAndSignData(
      params->properties.certificate,
      params->properties.public_key,
      params->properties.nonce,
      params->properties.signed_data,
      params->properties.device_serial,
      params->data,
      base::Bind(
          &NetworkingPrivateVerifyAndSignDataFunction::ResultCallback,
          this),
      base::Bind(
          &NetworkingPrivateVerifyAndSignDataFunction::ErrorCallback,
          this));
  return true;
}

void NetworkingPrivateVerifyAndSignDataFunction::ResultCallback(
    const std::string& result) {
  results_ = api::VerifyAndSignData::Results::Create(result);
  SendResponse(true);
}

void NetworkingPrivateVerifyAndSignDataFunction::ErrorCallback(
    const std::string& error_name, const std::string& error) {
  error_ = error_name;
  SendResponse(false);
}
