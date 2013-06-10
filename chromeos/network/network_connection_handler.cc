// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_connection_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/cert_loader.h"
#include "chromeos/network/certificate_pattern_matcher.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_ui_data.h"
#include "dbus/object_path.h"
#include "net/cert/x509_certificate.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void InvokeErrorCallback(const std::string& service_path,
                         const network_handler::ErrorCallback& error_callback,
                         const std::string& error_name) {
  std::string error_msg = "Connect Error: " + error_name;
  NET_LOG_ERROR(error_msg, service_path);
  if (error_callback.is_null())
    return;
  scoped_ptr<base::DictionaryValue> error_data(
      network_handler::CreateErrorData(service_path, error_name, error_msg));
  error_callback.Run(error_name, error_data.Pass());
}

bool NetworkConnectable(const NetworkState* network) {
  if (network->type() == flimflam::kTypeVPN)
    return false;  // TODO(stevenjb): Shill needs to properly set Connectable.
  return network->connectable();
}

bool NetworkMayNeedCredentials(const NetworkState* network) {
  if (network->type() == flimflam::kTypeWifi &&
      (network->security() == flimflam::kSecurity8021x ||
       network->security() == flimflam::kSecurityWep /* For dynamic WEP*/))
    return true;
  if (network->type() == flimflam::kTypeVPN)
    return true;
  return false;
}

bool NetworkRequiresActivation(const NetworkState* network) {
  return (network->type() == flimflam::kTypeCellular &&
          (network->activation_state() != flimflam::kActivationStateActivated ||
           network->cellular_out_of_credits()));
}

bool VPNIsConfigured(const std::string& service_path,
                     const base::DictionaryValue& service_properties) {
  const DictionaryValue* properties;
  if (!service_properties.GetDictionary(flimflam::kProviderProperty,
                                        &properties)) {
    NET_LOG_ERROR("VPN Provider Dictionary not present", service_path);
    return false;
  }
  std::string provider_type;
  // Note: we use Value path expansion to extract Provider.Type.
  if (!properties->GetString(flimflam::kTypeProperty, &provider_type)) {
    NET_LOG_ERROR("VPN Provider Type not present", service_path);
    return false;
  }
  if (provider_type == flimflam::kProviderOpenVpn) {
    std::string hostname;
    properties->GetString(flimflam::kHostProperty, &hostname);
    if (hostname.empty()) {
      NET_LOG_EVENT("OpenVPN: No hostname", service_path);
      return false;
    }
    std::string username;
    properties->GetStringWithoutPathExpansion(
        flimflam::kOpenVPNUserProperty, &username);
    if (username.empty()) {
      NET_LOG_EVENT("OpenVPN: No username", service_path);
      return false;
    }
    bool passphrase_required = false;
    properties->GetBooleanWithoutPathExpansion(
        flimflam::kPassphraseRequiredProperty, &passphrase_required);
    std::string passphrase;
    properties->GetStringWithoutPathExpansion(
        flimflam::kOpenVPNPasswordProperty, &passphrase);
    if (passphrase_required && passphrase.empty()) {
      NET_LOG_EVENT("OpenVPN: No passphrase", service_path);
      return false;
    }
    std::string client_cert_id;
    properties->GetStringWithoutPathExpansion(
        flimflam::kOpenVPNClientCertIdProperty, &client_cert_id);
    if (client_cert_id.empty()) {
      NET_LOG_EVENT("OpenVPN: No cert id", service_path);
      return false;
    }
    NET_LOG_EVENT("OpenVPN Is Configured", service_path);
  } else {
    bool passphrase_required = false;
    std::string passphrase;
    properties->GetBooleanWithoutPathExpansion(
        flimflam::kL2tpIpsecPskRequiredProperty, &passphrase_required);
    properties->GetStringWithoutPathExpansion(
        flimflam::kL2tpIpsecPskProperty, &passphrase);
    if (passphrase_required && passphrase.empty())
      return false;
    NET_LOG_EVENT("VPN Is Configured", service_path);
  }
  return true;
}

bool CertificateIsConfigured(NetworkUIData* ui_data) {
  if (ui_data->certificate_type() != CLIENT_CERT_TYPE_PATTERN)
    return true;  // No certificate or a reference.
  if (ui_data->onc_source() == onc::ONC_SOURCE_DEVICE_POLICY) {
    // We skip checking certificate patterns for device policy ONC so that an
    // unmanaged user can't get to the place where a cert is presented for them
    // involuntarily.
    return true;
  }
  if (ui_data->certificate_pattern().Empty())
    return false;

  // Find the matching certificate.
  scoped_refptr<net::X509Certificate> matching_cert =
      certificate_pattern::GetCertificateMatch(
          ui_data->certificate_pattern());
  if (!matching_cert.get())
    return false;
  return true;
}

}  // namespace

const char NetworkConnectionHandler::kErrorNotFound[] = "not-found";
const char NetworkConnectionHandler::kErrorConnected[] = "connected";
const char NetworkConnectionHandler::kErrorConnecting[] = "connecting";
const char NetworkConnectionHandler::kErrorNotConnected[] = "not-connected";
const char NetworkConnectionHandler::kErrorPassphraseRequired[] =
    "passphrase-required";
const char NetworkConnectionHandler::kErrorActivationRequired[] =
    "activation-required";
const char NetworkConnectionHandler::kErrorCertificateRequired[] =
    "certificate-required";
const char NetworkConnectionHandler::kErrorConfigurationRequired[] =
    "configuration-required";
const char NetworkConnectionHandler::kErrorShillError[] = "shill-error";
const char NetworkConnectionHandler::kErrorConnectFailed[] = "connect-failed";
const char NetworkConnectionHandler::kErrorUnknown[] = "unknown-error";

struct NetworkConnectionHandler::ConnectRequest {
  ConnectRequest(const base::Closure& success,
                 const network_handler::ErrorCallback& error)
      : connect_state(CONNECT_REQUESTED),
        success_callback(success),
        error_callback(error) {
  }
  enum ConnectState {
    CONNECT_REQUESTED = 0,
    CONNECT_STARTED = 1,
    CONNECT_CONNECTING = 2
  };
  ConnectState connect_state;
  base::Closure success_callback;
  network_handler::ErrorCallback error_callback;
};

NetworkConnectionHandler::NetworkConnectionHandler()
    : network_state_handler_(NULL),
      network_configuration_handler_(NULL) {
  const char* new_handler_enabled =
      CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kUseNewNetworkConnectionHandler) ?
      "enabled" : "disabled";
  NET_LOG_EVENT("NewNetworkConnectionHandler", new_handler_enabled);
}

NetworkConnectionHandler::~NetworkConnectionHandler() {
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this);
}

void NetworkConnectionHandler::Init(
    NetworkStateHandler* network_state_handler,
    NetworkConfigurationHandler* network_configuration_handler) {
  if (network_state_handler) {
    network_state_handler_ = network_state_handler;
    network_state_handler_->AddObserver(this);
  }
  network_configuration_handler_ = network_configuration_handler;
}

void NetworkConnectionHandler::ConnectToNetwork(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback,
    bool ignore_error_state) {
  NET_LOG_USER("ConnectToNetwork", service_path);
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!network) {
    InvokeErrorCallback(service_path, error_callback, kErrorNotFound);
    return;
  }
  if (HasConnectingNetwork(service_path)) {
    NET_LOG_USER("Connect Request While Pending", service_path);
    InvokeErrorCallback(service_path, error_callback, kErrorConnecting);
    return;
  }
  if (network->IsConnectedState()) {
    InvokeErrorCallback(service_path, error_callback, kErrorConnected);
    return;
  }
  if (network->IsConnectingState()) {
    InvokeErrorCallback(service_path, error_callback, kErrorConnecting);
    return;
  }
  if (NetworkRequiresActivation(network)) {
    InvokeErrorCallback(service_path, error_callback, kErrorActivationRequired);
    return;
  }
  if (!ignore_error_state) {
    if (network->passphrase_required()) {
      InvokeErrorCallback(service_path, error_callback,
                          kErrorPassphraseRequired);
      return;
    }
    if (network->error() == flimflam::kErrorConnectFailed) {
      InvokeErrorCallback(service_path, error_callback,
                          kErrorPassphraseRequired);
      return;
    }
    if (network->error() == flimflam::kErrorBadPassphrase) {
      InvokeErrorCallback(service_path, error_callback,
                          kErrorPassphraseRequired);
      return;
    }
    if (network->HasAuthenticationError()) {
      InvokeErrorCallback(service_path, error_callback,
                          kErrorConfigurationRequired);
      return;
    }
  }

  // All synchronous checks passed, add |service_path| to connecting list.
  pending_requests_.insert(std::make_pair(
      service_path, ConnectRequest(success_callback, error_callback)));

  if (!NetworkConnectable(network) && NetworkMayNeedCredentials(network)) {
    // Request additional properties to check.
    network_configuration_handler_->GetProperties(
        network->path(),
        base::Bind(&NetworkConnectionHandler::VerifyConfiguredAndConnect,
                   AsWeakPtr()),
        base::Bind(&NetworkConnectionHandler::HandleConfigurationFailure,
                   AsWeakPtr(), network->path()));
    return;
  }
  // All checks passed, send connect request.
  CallShillConnect(service_path);
}

void NetworkConnectionHandler::DisconnectNetwork(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG_USER("DisconnectNetwork", service_path);
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!network) {
    InvokeErrorCallback(service_path, error_callback, kErrorNotFound);
    return;
  }
  if (!network->IsConnectedState()) {
    InvokeErrorCallback(service_path, error_callback, kErrorNotConnected);
    return;
  }
  CallShillDisconnect(service_path, success_callback, error_callback);
}

bool NetworkConnectionHandler::HasConnectingNetwork(
    const std::string& service_path) {
  return pending_requests_.count(service_path) != 0;
}

void NetworkConnectionHandler::NetworkListChanged() {
  CheckAllPendingRequests();
}

void NetworkConnectionHandler::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (HasConnectingNetwork(network->path()))
    CheckPendingRequest(network->path());
}

NetworkConnectionHandler::ConnectRequest*
NetworkConnectionHandler::pending_request(
    const std::string& service_path) {
  std::map<std::string, ConnectRequest>::iterator iter =
      pending_requests_.find(service_path);
  return iter != pending_requests_.end() ? &(iter->second) : NULL;
}

// ConnectToNetwork implementation

void NetworkConnectionHandler::VerifyConfiguredAndConnect(
    const std::string& service_path,
    const base::DictionaryValue& properties) {
  NET_LOG_EVENT("VerifyConfiguredAndConnect", service_path);
  ConnectRequest* request = pending_request(service_path);
  DCHECK(request);
  network_handler::ErrorCallback error_callback = request->error_callback;

  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!network) {
    pending_requests_.erase(service_path);
    InvokeErrorCallback(service_path, error_callback, kErrorNotFound);
    return;
  }

  // VPN requires a host and username to be set.
  if (network->type() == flimflam::kTypeVPN &&
      !VPNIsConfigured(service_path, properties)) {
    pending_requests_.erase(service_path);
    InvokeErrorCallback(service_path, error_callback,
                        kErrorConfigurationRequired);
    return;
  }

  // Check certificate properties in kUIDataProperty.
  scoped_ptr<NetworkUIData> ui_data =
      ManagedNetworkConfigurationHandler::GetUIData(properties);
  if (ui_data && !CertificateIsConfigured(ui_data.get())) {
    pending_requests_.erase(service_path);
    InvokeErrorCallback(service_path, error_callback,
                        kErrorCertificateRequired);
    return;
  }

  CallShillConnect(service_path);
}

void NetworkConnectionHandler::CallShillConnect(
    const std::string& service_path) {
  NET_LOG_EVENT("Connect Request", service_path);
  DBusThreadManager::Get()->GetShillServiceClient()->Connect(
      dbus::ObjectPath(service_path),
      base::Bind(&NetworkConnectionHandler::HandleShillConnectSuccess,
                 AsWeakPtr(), service_path),
      base::Bind(&NetworkConnectionHandler::HandleShillConnectFailure,
                 AsWeakPtr(), service_path));
}

void NetworkConnectionHandler::HandleConfigurationFailure(
    const std::string& service_path,
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  ConnectRequest* request = pending_request(service_path);
  DCHECK(request);
  network_handler::ErrorCallback error_callback = request->error_callback;
  pending_requests_.erase(service_path);
  if (!error_callback.is_null())
    error_callback.Run(error_name, error_data.Pass());
}

void NetworkConnectionHandler::HandleShillConnectSuccess(
    const std::string& service_path) {
  ConnectRequest* request = pending_request(service_path);
  DCHECK(request);
  request->connect_state = ConnectRequest::CONNECT_STARTED;
  NET_LOG_EVENT("Connect Request Sent", service_path);
  // Do not call success_callback here, wait for one of the following
  // conditions:
  // * State transitions to a non connecting state indicating succes or failure
  // * Network is no longer in the visible list, indicating failure
  CheckPendingRequest(service_path);
}

void NetworkConnectionHandler::HandleShillConnectFailure(
    const std::string& service_path,
    const std::string& error_name,
    const std::string& error_message) {
  ConnectRequest* request = pending_request(service_path);
  DCHECK(request);
  network_handler::ErrorCallback error_callback = request->error_callback;
  pending_requests_.erase(service_path);
  std::string error = "Connect Failure: " + error_name + ": " + error_message;
  network_handler::ShillErrorCallbackFunction(
      service_path, error_callback, error_name, error_message);
}

void NetworkConnectionHandler::CheckPendingRequest(
    const std::string service_path) {
  ConnectRequest* request = pending_request(service_path);
  DCHECK(request);
  if (request->connect_state == ConnectRequest::CONNECT_REQUESTED)
    return;  // Request has not started, ignore update
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!network) {
    network_handler::ErrorCallback error_callback = request->error_callback;
    pending_requests_.erase(service_path);
    InvokeErrorCallback(service_path, error_callback, kErrorNotFound);
    return;
  }
  if (network->IsConnectingState()) {
    request->connect_state = ConnectRequest::CONNECT_CONNECTING;
    return;
  }
  if (network->IsConnectedState()) {
    NET_LOG_EVENT("Connect Request Succeeded", service_path);
    if (!request->success_callback.is_null())
      request->success_callback.Run();
    pending_requests_.erase(service_path);
    return;
  }
  if (network->connection_state() == flimflam::kStateIdle &&
      request->connect_state != ConnectRequest::CONNECT_CONNECTING) {
    // Connection hasn't started yet, keep waiting.
    return;
  }

  // Network is neither connecting or connected; an error occurred.
  std::string error_name = kErrorConnectFailed;
  std::string error_detail = network->error();
  if (error_detail.empty()) {
    if (network->connection_state() == flimflam::kStateFailure)
      error_detail = flimflam::kUnknownString;
    else
      error_detail = "Unexpected State: " + network->connection_state();
  }
  std::string error_msg = error_name + ": " + error_detail;
  NET_LOG_ERROR(error_msg, service_path);

  network_handler::ErrorCallback error_callback = request->error_callback;
  pending_requests_.erase(service_path);
  if (error_callback.is_null())
    return;
  scoped_ptr<base::DictionaryValue> error_data(
      network_handler::CreateErrorData(service_path, error_name, error_msg));
  error_callback.Run(error_name, error_data.Pass());
}

void NetworkConnectionHandler::CheckAllPendingRequests() {
  for (std::map<std::string, ConnectRequest>::iterator iter =
           pending_requests_.begin(); iter != pending_requests_.end(); ++iter) {
    CheckPendingRequest(iter->first);
  }
}

// Disconnect

void NetworkConnectionHandler::CallShillDisconnect(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG_USER("Disconnect Request", service_path);
  DBusThreadManager::Get()->GetShillServiceClient()->Disconnect(
      dbus::ObjectPath(service_path),
      base::Bind(&NetworkConnectionHandler::HandleShillDisconnectSuccess,
                 AsWeakPtr(), service_path, success_callback),
      base::Bind(&NetworkConnectionHandler::HandleShillDisconnectFailure,
                 AsWeakPtr(), service_path, error_callback));
}

void NetworkConnectionHandler::HandleShillDisconnectSuccess(
    const std::string& service_path,
    const base::Closure& success_callback) {
  NET_LOG_EVENT("Disconnect Request Sent", service_path);
  success_callback.Run();
}

void NetworkConnectionHandler::HandleShillDisconnectFailure(
    const std::string& service_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  std::string error =
      "Disconnect Failure: " + error_name + ": " + error_message;
  network_handler::ShillErrorCallbackFunction(
      service_path, error_callback, error_name, error_message);
}

}  // namespace chromeos
