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

bool VPNIsConfigured(const base::DictionaryValue& properties) {
  std::string provider_type;
  // Note: we use Value path expansion to extract Provider.Type.
  properties.GetString(flimflam::kProviderTypeProperty, &provider_type);
  if (provider_type == flimflam::kProviderOpenVpn) {
    std::string hostname;
    properties.GetString(flimflam::kProviderHostProperty, &hostname);
    if (hostname.empty())
      return false;
    std::string username;
    properties.GetStringWithoutPathExpansion(
        flimflam::kOpenVPNUserProperty, &username);
    if (username.empty())
      return false;
    std::string client_cert_id;
    properties.GetStringWithoutPathExpansion(
        flimflam::kOpenVPNClientCertIdProperty, &client_cert_id);
    if (client_cert_id.empty())
      return false;
  } else {
    bool passphrase_required = false;
    std::string passphrase;
    properties.GetBooleanWithoutPathExpansion(
        flimflam::kL2tpIpsecPskRequiredProperty, &passphrase_required);
    properties.GetStringWithoutPathExpansion(
        flimflam::kL2tpIpsecPskProperty, &passphrase);
    if (passphrase_required && passphrase.empty())
      return false;
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

static NetworkConnectionHandler* g_connection_handler_instance = NULL;

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

// static
void NetworkConnectionHandler::Initialize() {
  CHECK(!g_connection_handler_instance);
  g_connection_handler_instance = new NetworkConnectionHandler;
}

// static
void NetworkConnectionHandler::Shutdown() {
  CHECK(g_connection_handler_instance);
  delete g_connection_handler_instance;
  g_connection_handler_instance = NULL;
}

// static
NetworkConnectionHandler* NetworkConnectionHandler::Get() {
  CHECK(g_connection_handler_instance)
      << "NetworkConnectionHandler::Get() called before Initialize()";
  return g_connection_handler_instance;
}

NetworkConnectionHandler::NetworkConnectionHandler() {
  const char* new_handlers_enabled =
      CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kUseNewNetworkConfigurationHandlers) ?
      "enabled" : "disabled";
  NET_LOG_EVENT("NewNetworkConfigurationHandlers", new_handlers_enabled);
}

NetworkConnectionHandler::~NetworkConnectionHandler() {
}

void NetworkConnectionHandler::ConnectToNetwork(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback,
    bool ignore_error_state) {
  const NetworkState* network =
      NetworkStateHandler::Get()->GetNetworkState(service_path);
  if (!network) {
    InvokeErrorCallback(service_path, error_callback, kErrorNotFound);
    return;
  }
  if (network->IsConnectedState()) {
    InvokeErrorCallback(service_path, error_callback, kErrorConnected);
    return;
  }
  if (network->IsConnectingState() ||
      pending_requests_.find(service_path) != pending_requests_.end()) {
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
  pending_requests_.insert(service_path);

  if (!network->connectable() && NetworkMayNeedCredentials(network)) {
    // Request additional properties to check.
    NetworkConfigurationHandler::Get()->GetProperties(
        network->path(),
        base::Bind(&NetworkConnectionHandler::VerifyConfiguredAndConnect,
                   AsWeakPtr(), success_callback, error_callback),
        base::Bind(&NetworkConnectionHandler::HandleConfigurationFailure,
                   AsWeakPtr(), network->path(), error_callback));
    return;
  }
  // All checks passed, send connect request.
  CallShillConnect(service_path, success_callback, error_callback);
}

void NetworkConnectionHandler::DisconnectNetwork(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  const NetworkState* network =
      NetworkStateHandler::Get()->GetNetworkState(service_path);
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

void NetworkConnectionHandler::CallShillConnect(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  // TODO(stevenjb): Remove SetConnectingNetwork and use this class to maintain
  // the connecting network(s) once NetworkLibrary path is eliminated.
  NetworkStateHandler::Get()->SetConnectingNetwork(service_path);
  NET_LOG_EVENT("Connect Request", service_path);
  DBusThreadManager::Get()->GetShillServiceClient()->Connect(
      dbus::ObjectPath(service_path),
      base::Bind(&NetworkConnectionHandler::HandleShillSuccess,
                 AsWeakPtr(), service_path, success_callback),
      base::Bind(&NetworkConnectionHandler::HandleShillFailure,
                 AsWeakPtr(), service_path, error_callback));
}

void NetworkConnectionHandler::CallShillDisconnect(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG_EVENT("Disconnect Request", service_path);
  DBusThreadManager::Get()->GetShillServiceClient()->Disconnect(
      dbus::ObjectPath(service_path),
      base::Bind(&NetworkConnectionHandler::HandleShillSuccess,
                 AsWeakPtr(), service_path, success_callback),
      base::Bind(&NetworkConnectionHandler::HandleShillFailure,
                 AsWeakPtr(), service_path, error_callback));
}

void NetworkConnectionHandler::VerifyConfiguredAndConnect(
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback,
    const std::string& service_path,
    const base::DictionaryValue& properties) {
  const NetworkState* network =
      NetworkStateHandler::Get()->GetNetworkState(service_path);
  if (!network) {
    InvokeErrorCallback(service_path, error_callback, kErrorNotFound);
    return;
  }

  // VPN requires a host and username to be set.
  if (network->type() == flimflam::kTypeVPN &&
      !VPNIsConfigured(properties)) {
    InvokeErrorCallback(service_path, error_callback,
                        kErrorConfigurationRequired);
    return;
  }

  // Check certificate properties in kUIDataProperty.
  scoped_ptr<NetworkUIData> ui_data =
      ManagedNetworkConfigurationHandler::GetUIData(properties);
  if (ui_data && !CertificateIsConfigured(ui_data.get())) {
    InvokeErrorCallback(service_path, error_callback,
                        kErrorCertificateRequired);
    return;
  }

  CallShillConnect(service_path, success_callback, error_callback);
}

void NetworkConnectionHandler::HandleConfigurationFailure(
    const std::string& service_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  pending_requests_.erase(service_path);
  if (!error_callback.is_null())
    error_callback.Run(error_name, error_data.Pass());
}

void NetworkConnectionHandler::HandleShillSuccess(
    const std::string& service_path,
    const base::Closure& success_callback) {
  // TODO(stevenjb): Currently, this only indicates that the connect request
  // succeeded. It might be preferable to wait for the actually connect
  // attempt to succeed or fail here and only call |success_callback| at that
  // point (or maybe call it twice, once indicating in-progress, then success
  // or failure).
  pending_requests_.erase(service_path);
  NET_LOG_EVENT("Connect Request Sent", service_path);
  success_callback.Run();
}

void NetworkConnectionHandler::HandleShillFailure(
    const std::string& service_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  pending_requests_.erase(service_path);
  std::string error = "Connect Failure: " + error_name + ": " + error_message;
  network_handler::ShillErrorCallbackFunction(
      service_path, error_callback, error_name, error_message);
}

}  // namespace chromeos
