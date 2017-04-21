// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_connection_handler.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/certificate_pattern.h"
#include "chromeos/network/client_cert_resolver.h"
#include "chromeos/network/client_cert_util.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/shill_property_util.h"
#include "dbus/object_path.h"
#include "net/cert/x509_certificate.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

bool IsAuthenticationError(const std::string& error) {
  return (error == shill::kErrorBadWEPKey ||
          error == shill::kErrorPppAuthFailed ||
          error == shill::kErrorEapLocalTlsFailed ||
          error == shill::kErrorEapRemoteTlsFailed ||
          error == shill::kErrorEapAuthenticationFailed);
}

bool VPNRequiresCredentials(const std::string& service_path,
                           const std::string& provider_type,
                           const base::DictionaryValue& provider_properties) {
  if (provider_type == shill::kProviderOpenVpn) {
    std::string username;
    provider_properties.GetStringWithoutPathExpansion(
        shill::kOpenVPNUserProperty, &username);
    if (username.empty()) {
      NET_LOG_EVENT("OpenVPN: No username", service_path);
      return true;
    }
    bool passphrase_required = false;
    provider_properties.GetBooleanWithoutPathExpansion(
        shill::kPassphraseRequiredProperty, &passphrase_required);
    if (passphrase_required) {
      NET_LOG_EVENT("OpenVPN: Passphrase Required", service_path);
      return true;
    }
    NET_LOG_EVENT("OpenVPN Is Configured", service_path);
  } else {
    bool passphrase_required = false;
    provider_properties.GetBooleanWithoutPathExpansion(
        shill::kL2tpIpsecPskRequiredProperty, &passphrase_required);
    if (passphrase_required) {
      NET_LOG_EVENT("VPN: PSK Required", service_path);
      return true;
    }
    provider_properties.GetBooleanWithoutPathExpansion(
        shill::kPassphraseRequiredProperty, &passphrase_required);
    if (passphrase_required) {
      NET_LOG_EVENT("VPN: Passphrase Required", service_path);
      return true;
    }
    NET_LOG_EVENT("VPN Is Configured", service_path);
  }
  return false;
}

std::string GetDefaultUserProfilePath(const NetworkState* network) {
  if (!NetworkHandler::IsInitialized() ||
      (LoginState::IsInitialized() &&
       !LoginState::Get()->UserHasNetworkProfile()) ||
      (network && network->type() == shill::kTypeWifi &&
       network->security_class() == shill::kSecurityNone)) {
    return NetworkProfileHandler::GetSharedProfilePath();
  }
  const NetworkProfile* profile  =
      NetworkHandler::Get()->network_profile_handler()->GetDefaultUserProfile();
  return profile ? profile->path
                 : NetworkProfileHandler::GetSharedProfilePath();
}

}  // namespace

const char NetworkConnectionHandler::kErrorNotFound[] = "not-found";
const char NetworkConnectionHandler::kErrorConnected[] = "connected";
const char NetworkConnectionHandler::kErrorConnecting[] = "connecting";
const char NetworkConnectionHandler::kErrorNotConnected[] = "not-connected";
const char NetworkConnectionHandler::kErrorPassphraseRequired[] =
    "passphrase-required";
const char NetworkConnectionHandler::kErrorBadPassphrase[] = "bad-passphrase";
const char NetworkConnectionHandler::kErrorCertificateRequired[] =
    "certificate-required";
const char NetworkConnectionHandler::kErrorConfigurationRequired[] =
    "configuration-required";
const char NetworkConnectionHandler::kErrorAuthenticationRequired[] =
    "authentication-required";
const char NetworkConnectionHandler::kErrorConnectFailed[] = "connect-failed";
const char NetworkConnectionHandler::kErrorDisconnectFailed[] =
    "disconnect-failed";
const char NetworkConnectionHandler::kErrorConfigureFailed[] =
    "configure-failed";
const char NetworkConnectionHandler::kErrorConnectCanceled[] =
    "connect-canceled";
const char NetworkConnectionHandler::kErrorCertLoadTimeout[] =
    "cert-load-timeout";
const char NetworkConnectionHandler::kErrorUnmanagedNetwork[] =
    "unmanaged-network";
const char NetworkConnectionHandler::kErrorActivateFailed[] = "activate-failed";

struct NetworkConnectionHandler::ConnectRequest {
  ConnectRequest(const std::string& service_path,
                 const std::string& profile_path,
                 const base::Closure& success,
                 const network_handler::ErrorCallback& error)
      : service_path(service_path),
        profile_path(profile_path),
        connect_state(CONNECT_REQUESTED),
        success_callback(success),
        error_callback(error) {
  }
  enum ConnectState {
    CONNECT_REQUESTED = 0,
    CONNECT_STARTED = 1,
    CONNECT_CONNECTING = 2
  };
  std::string service_path;
  std::string profile_path;
  ConnectState connect_state;
  base::Closure success_callback;
  network_handler::ErrorCallback error_callback;
};

NetworkConnectionHandler::NetworkConnectionHandler()
    : cert_loader_(NULL),
      network_state_handler_(NULL),
      configuration_handler_(NULL),
      logged_in_(false),
      certificates_loaded_(false) {
}

NetworkConnectionHandler::~NetworkConnectionHandler() {
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  if (cert_loader_)
    cert_loader_->RemoveObserver(this);
  if (LoginState::IsInitialized())
    LoginState::Get()->RemoveObserver(this);
}

void NetworkConnectionHandler::Init(
    NetworkStateHandler* network_state_handler,
    NetworkConfigurationHandler* network_configuration_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler) {
  if (LoginState::IsInitialized())
    LoginState::Get()->AddObserver(this);

  if (CertLoader::IsInitialized()) {
    cert_loader_ = CertLoader::Get();
    cert_loader_->AddObserver(this);
    if (cert_loader_->certificates_loaded()) {
      NET_LOG_EVENT("Certificates Loaded", "");
      certificates_loaded_ = true;
    }
  } else {
    // TODO(tbarzic): Require a mock or stub cert_loader in tests.
    NET_LOG_EVENT("Certificate Loader not initialized", "");
    certificates_loaded_ = true;
  }

  if (network_state_handler) {
    network_state_handler_ = network_state_handler;
    network_state_handler_->AddObserver(this, FROM_HERE);
  }
  configuration_handler_ = network_configuration_handler;
  managed_configuration_handler_ = managed_network_configuration_handler;

  // After this point, the NetworkConnectionHandler is fully initialized (all
  // handler references set, observers registered, ...).

  if (LoginState::IsInitialized())
    LoggedInStateChanged();
}

void NetworkConnectionHandler::AddObserver(
    NetworkConnectionObserver* observer) {
  observers_.AddObserver(observer);
}

void NetworkConnectionHandler::RemoveObserver(
    NetworkConnectionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkConnectionHandler::LoggedInStateChanged() {
  LoginState* login_state = LoginState::Get();
  if (logged_in_ || !login_state->IsUserLoggedIn())
    return;

  logged_in_ = true;
  logged_in_time_ = base::TimeTicks::Now();
}

void NetworkConnectionHandler::OnCertificatesLoaded(
    const net::CertificateList& cert_list,
    bool initial_load) {
  certificates_loaded_ = true;
  NET_LOG_EVENT("Certificates Loaded", "");
  if (queued_connect_)
    ConnectToQueuedNetwork();
}

void NetworkConnectionHandler::ConnectToNetwork(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback,
    bool check_error_state) {
  NET_LOG_USER("ConnectToNetwork", service_path);
  for (auto& observer : observers_)
    observer.ConnectToNetworkRequested(service_path);

  // Clear any existing queued connect request.
  queued_connect_.reset();
  if (HasConnectingNetwork(service_path)) {
    NET_LOG_USER("Connect Request While Pending", service_path);
    InvokeConnectErrorCallback(service_path, error_callback, kErrorConnecting);
    return;
  }

  // Check cached network state for connected, connecting, or unactivated
  // networks. These states will not be affected by a recent configuration.
  // Note: NetworkState may not exist for a network that was recently
  // configured, in which case these checks do not apply anyway.
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);

  if (network) {
    // For existing networks, perform some immediate consistency checks.
    if (network->IsConnectedState()) {
      InvokeConnectErrorCallback(service_path, error_callback, kErrorConnected);
      return;
    }
    if (network->IsConnectingState()) {
      InvokeConnectErrorCallback(service_path, error_callback,
                                 kErrorConnecting);
      return;
    }

    if (check_error_state) {
      const std::string& error = network->last_error();
      if (error == shill::kErrorBadPassphrase) {
        InvokeConnectErrorCallback(service_path, error_callback,
                                   kErrorBadPassphrase);
        return;
      }
      if (IsAuthenticationError(error)) {
        InvokeConnectErrorCallback(service_path, error_callback,
                                   kErrorAuthenticationRequired);
        return;
      }
    }
  }

  // If the network does not have a profile path, specify the correct default
  // profile here and set it once connected. Otherwise leave it empty to
  // indicate that it does not need to be set.
  std::string profile_path;
  if (!network || network->profile_path().empty())
    profile_path = GetDefaultUserProfilePath(network);

  // All synchronous checks passed, add |service_path| to connecting list.
  pending_requests_.insert(std::make_pair(
      service_path,
      ConnectRequest(service_path, profile_path,
                     success_callback, error_callback)));

  // Connect immediately to 'connectable' networks.
  // TODO(stevenjb): Shill needs to properly set Connectable for VPN.
  if (network && network->connectable() && network->type() != shill::kTypeVPN) {
    if (IsNetworkProhibitedByPolicy(network->type(), network->guid(),
                                    network->profile_path())) {
      ErrorCallbackForPendingRequest(service_path, kErrorUnmanagedNetwork);
      return;
    }

    CallShillConnect(service_path);
    return;
  }

  // Request additional properties to check. VerifyConfiguredAndConnect will
  // use only these properties, not cached properties, to ensure that they
  // are up to date after any recent configuration.
  configuration_handler_->GetShillProperties(
      service_path,
      base::Bind(&NetworkConnectionHandler::VerifyConfiguredAndConnect,
                 AsWeakPtr(), check_error_state),
      base::Bind(&NetworkConnectionHandler::HandleConfigurationFailure,
                 AsWeakPtr(), service_path));
}

void NetworkConnectionHandler::DisconnectNetwork(
    const std::string& service_path,
    const base::Closure& success_callback,
    const network_handler::ErrorCallback& error_callback) {
  NET_LOG_USER("DisconnectNetwork", service_path);
  for (auto& observer : observers_)
    observer.DisconnectRequested(service_path);

  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!network) {
    NET_LOG_ERROR("Disconnect Error: Not Found", service_path);
    network_handler::RunErrorCallback(error_callback, service_path,
                                      kErrorNotFound, "");
    return;
  }
  if (!network->IsConnectedState() && !network->IsConnectingState()) {
    NET_LOG_ERROR("Disconnect Error: Not Connected", service_path);
    network_handler::RunErrorCallback(error_callback, service_path,
                                      kErrorNotConnected, "");
    return;
  }
  pending_requests_.erase(service_path);
  CallShillDisconnect(service_path, success_callback, error_callback);
}

bool NetworkConnectionHandler::HasConnectingNetwork(
    const std::string& service_path) {
  return pending_requests_.count(service_path) != 0;
}

bool NetworkConnectionHandler::HasPendingConnectRequest() {
  return pending_requests_.size() > 0;
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
NetworkConnectionHandler::GetPendingRequest(const std::string& service_path) {
  std::map<std::string, ConnectRequest>::iterator iter =
      pending_requests_.find(service_path);
  return iter != pending_requests_.end() ? &(iter->second) : NULL;
}

// ConnectToNetwork implementation

void NetworkConnectionHandler::VerifyConfiguredAndConnect(
    bool check_error_state,
    const std::string& service_path,
    const base::DictionaryValue& service_properties) {
  NET_LOG_EVENT("VerifyConfiguredAndConnect", service_path);

  // If 'passphrase_required' is still true, then the 'Passphrase' property
  // has not been set to a minimum length value.
  bool passphrase_required = false;
  service_properties.GetBooleanWithoutPathExpansion(
      shill::kPassphraseRequiredProperty, &passphrase_required);
  if (passphrase_required) {
    ErrorCallbackForPendingRequest(service_path, kErrorPassphraseRequired);
    return;
  }

  std::string type, security_class;
  service_properties.GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
  service_properties.GetStringWithoutPathExpansion(
      shill::kSecurityClassProperty, &security_class);
  bool connectable = false;
  service_properties.GetBooleanWithoutPathExpansion(
      shill::kConnectableProperty, &connectable);

  // In case NetworkState was not available in ConnectToNetwork (e.g. it had
  // been recently configured), we need to check Connectable again.
  if (connectable && type != shill::kTypeVPN) {
    // TODO(stevenjb): Shill needs to properly set Connectable for VPN.
    CallShillConnect(service_path);
    return;
  }

  // Get VPN provider type and host (required for configuration) and ensure
  // that required VPN non-cert properties are set.
  const base::DictionaryValue* provider_properties = NULL;
  std::string vpn_provider_type, vpn_provider_host, vpn_client_cert_id;
  if (type == shill::kTypeVPN) {
    // VPN Provider values are read from the "Provider" dictionary, not the
    // "Provider.Type", etc keys (which are used only to set the values).
    if (service_properties.GetDictionaryWithoutPathExpansion(
            shill::kProviderProperty, &provider_properties)) {
      provider_properties->GetStringWithoutPathExpansion(
          shill::kTypeProperty, &vpn_provider_type);
      provider_properties->GetStringWithoutPathExpansion(
          shill::kHostProperty, &vpn_provider_host);
      provider_properties->GetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertIdProperty, &vpn_client_cert_id);
    }
    if (vpn_provider_type.empty() || vpn_provider_host.empty()) {
      ErrorCallbackForPendingRequest(service_path, kErrorConfigurationRequired);
      return;
    }
  }

  std::string guid;
  service_properties.GetStringWithoutPathExpansion(shill::kGuidProperty, &guid);
  std::string profile;
  service_properties.GetStringWithoutPathExpansion(shill::kProfileProperty,
                                                   &profile);
  const base::DictionaryValue* user_policy =
      managed_configuration_handler_->FindPolicyByGuidAndProfile(guid, profile);

  if (IsNetworkProhibitedByPolicy(type, guid, profile)) {
    ErrorCallbackForPendingRequest(service_path, kErrorUnmanagedNetwork);
    return;
  }

  client_cert::ClientCertConfig cert_config_from_policy;
  if (user_policy)
    client_cert::OncToClientCertConfig(*user_policy, &cert_config_from_policy);

  client_cert::ConfigType client_cert_type = client_cert::CONFIG_TYPE_NONE;
  if (type == shill::kTypeVPN) {
    if (vpn_provider_type == shill::kProviderOpenVpn) {
      client_cert_type = client_cert::CONFIG_TYPE_OPENVPN;
    } else {
      // L2TP/IPSec only requires a certificate if one is specified in ONC
      // or one was configured by the UI. Otherwise it is L2TP/IPSec with
      // PSK and doesn't require a certificate.
      //
      // TODO(benchan): Modify shill to specify the authentication type via
      // the kL2tpIpsecAuthenticationType property, so that Chrome doesn't need
      // to deduce the authentication type based on the
      // kL2tpIpsecClientCertIdProperty here (and also in VPNConfigView).
      if (!vpn_client_cert_id.empty() ||
          cert_config_from_policy.client_cert_type !=
              onc::client_cert::kClientCertTypeNone) {
        client_cert_type = client_cert::CONFIG_TYPE_IPSEC;
      }
    }
  } else if (type == shill::kTypeWifi &&
             security_class == shill::kSecurity8021x) {
    client_cert_type = client_cert::CONFIG_TYPE_EAP;
  }

  base::DictionaryValue config_properties;
  if (client_cert_type != client_cert::CONFIG_TYPE_NONE) {
    // Note: if we get here then a certificate *may* be required, so we want
    // to ensure that certificates have loaded successfully before attempting
    // to connect.

    // User must be logged in to connect to a network requiring a certificate.
    if (!logged_in_ || !cert_loader_) {
      NET_LOG_ERROR("User not logged in", "");
      ErrorCallbackForPendingRequest(service_path, kErrorCertificateRequired);
      return;
    }
    // If certificates have not been loaded yet, queue the connect request.
    if (!certificates_loaded_) {
      NET_LOG_EVENT("Certificates not loaded", "");
      QueueConnectRequest(service_path);
      return;
    }

    // Check certificate properties from policy.
    if (cert_config_from_policy.client_cert_type ==
        onc::client_cert::kPattern) {
      if (!ClientCertResolver::ResolveCertificatePatternSync(
              client_cert_type,
              cert_config_from_policy.pattern,
              &config_properties)) {
        ErrorCallbackForPendingRequest(service_path, kErrorCertificateRequired);
        return;
      }
    } else if (check_error_state &&
               !client_cert::IsCertificateConfigured(client_cert_type,
                                                     service_properties)) {
      // Network may not be configured.
      ErrorCallbackForPendingRequest(service_path, kErrorConfigurationRequired);
      return;
    }
  }

  if (type == shill::kTypeVPN) {
    // VPN may require a username, and/or passphrase to be set. (Check after
    // ensuring that any required certificates are configured).
    DCHECK(provider_properties);
    if (VPNRequiresCredentials(
            service_path, vpn_provider_type, *provider_properties)) {
      NET_LOG_USER("VPN Requires Credentials", service_path);
      ErrorCallbackForPendingRequest(service_path, kErrorConfigurationRequired);
      return;
    }

    // If it's L2TP/IPsec PSK, there is no properties to configure, so proceed
    // to connect.
    if (client_cert_type == client_cert::CONFIG_TYPE_NONE) {
      CallShillConnect(service_path);
      return;
    }
  }

  if (!config_properties.empty()) {
    NET_LOG_EVENT("Configuring Network", service_path);
    configuration_handler_->SetShillProperties(
        service_path, config_properties,
        NetworkConfigurationObserver::SOURCE_USER_ACTION,
        base::Bind(&NetworkConnectionHandler::CallShillConnect, AsWeakPtr(),
                   service_path),
        base::Bind(&NetworkConnectionHandler::HandleConfigurationFailure,
                   AsWeakPtr(), service_path));
    return;
  }

  // Otherwise, we probably still need to configure the network since
  // 'Connectable' is false. If |check_error_state| is true, signal an
  // error, otherwise attempt to connect to possibly gain additional error
  // state from Shill (or in case 'Connectable' is improperly unset).
  if (check_error_state)
    ErrorCallbackForPendingRequest(service_path, kErrorConfigurationRequired);
  else
    CallShillConnect(service_path);
}

bool NetworkConnectionHandler::IsNetworkProhibitedByPolicy(
    const std::string& type,
    const std::string& guid,
    const std::string& profile_path) {
  if (!logged_in_)
    return false;
  if (type != shill::kTypeWifi)
    return false;
  const base::DictionaryValue* global_network_config =
      managed_configuration_handler_->GetGlobalConfigFromPolicy(
          std::string() /* no username hash, device policy */);
  if (!global_network_config)
    return false;
  bool policy_prohibites = false;
  if (!global_network_config->GetBooleanWithoutPathExpansion(
          ::onc::global_network_config::kAllowOnlyPolicyNetworksToConnect,
          &policy_prohibites) ||
      !policy_prohibites) {
    return false;
  }
  return !managed_configuration_handler_->FindPolicyByGuidAndProfile(
      guid, profile_path);
}

void NetworkConnectionHandler::QueueConnectRequest(
    const std::string& service_path) {
  ConnectRequest* request = GetPendingRequest(service_path);
  if (!request) {
    NET_LOG_ERROR("No pending request to queue", service_path);
    return;
  }

  const int kMaxCertLoadTimeSeconds = 15;
  base::TimeDelta dtime = base::TimeTicks::Now() - logged_in_time_;
  if (dtime > base::TimeDelta::FromSeconds(kMaxCertLoadTimeSeconds)) {
    NET_LOG_ERROR("Certificate load timeout", service_path);
    InvokeConnectErrorCallback(service_path, request->error_callback,
                               kErrorCertLoadTimeout);
    return;
  }

  NET_LOG_EVENT("Connect Request Queued", service_path);
  queued_connect_.reset(new ConnectRequest(
      service_path, request->profile_path,
      request->success_callback, request->error_callback));
  pending_requests_.erase(service_path);

  // Post a delayed task to check to see if certificates have loaded. If they
  // haven't, and queued_connect_ has not been cleared (e.g. by a successful
  // connect request), cancel the request and notify the user.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&NetworkConnectionHandler::CheckCertificatesLoaded,
                            AsWeakPtr()),
      base::TimeDelta::FromSeconds(kMaxCertLoadTimeSeconds) - dtime);
}

void NetworkConnectionHandler::CheckCertificatesLoaded() {
  if (certificates_loaded_)
    return;
  // If queued_connect_ has been cleared (e.g. another connect request occurred
  // and wasn't queued), do nothing here.
  if (!queued_connect_)
    return;
  // Otherwise, notify the user.
  NET_LOG_ERROR("Certificate load timeout", queued_connect_->service_path);
  InvokeConnectErrorCallback(queued_connect_->service_path,
                             queued_connect_->error_callback,
                             kErrorCertLoadTimeout);
  queued_connect_.reset();
}

void NetworkConnectionHandler::ConnectToQueuedNetwork() {
  DCHECK(queued_connect_);

  // Make a copy of |queued_connect_| parameters, because |queued_connect_|
  // will get reset at the beginning of |ConnectToNetwork|.
  std::string service_path = queued_connect_->service_path;
  base::Closure success_callback = queued_connect_->success_callback;
  network_handler::ErrorCallback error_callback =
      queued_connect_->error_callback;

  NET_LOG_EVENT("Connecting to Queued Network", service_path);
  ConnectToNetwork(service_path, success_callback, error_callback,
                   false /* check_error_state */);
}

void NetworkConnectionHandler::CallShillConnect(
    const std::string& service_path) {
  NET_LOG_EVENT("Sending Connect Request to Shill", service_path);
  network_state_handler_->ClearLastErrorForNetwork(service_path);
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
    std::unique_ptr<base::DictionaryValue> error_data) {
  ConnectRequest* request = GetPendingRequest(service_path);
  if (!request) {
    NET_LOG_ERROR("HandleConfigurationFailure called with no pending request.",
                  service_path);
    return;
  }
  network_handler::ErrorCallback error_callback = request->error_callback;
  pending_requests_.erase(service_path);
  InvokeConnectErrorCallback(service_path, error_callback,
                             kErrorConfigureFailed);
}

void NetworkConnectionHandler::HandleShillConnectSuccess(
    const std::string& service_path) {
  ConnectRequest* request = GetPendingRequest(service_path);
  if (!request) {
    NET_LOG_ERROR("HandleShillConnectSuccess called with no pending request.",
                  service_path);
    return;
  }
  request->connect_state = ConnectRequest::CONNECT_STARTED;
  NET_LOG_EVENT("Connect Request Acknowledged", service_path);
  // Do not call success_callback here, wait for one of the following
  // conditions:
  // * State transitions to a non connecting state indicating success or failure
  // * Network is no longer in the visible list, indicating failure
  CheckPendingRequest(service_path);
}

void NetworkConnectionHandler::HandleShillConnectFailure(
    const std::string& service_path,
    const std::string& dbus_error_name,
    const std::string& dbus_error_message) {
  ConnectRequest* request = GetPendingRequest(service_path);
  if (!request) {
    NET_LOG_ERROR("HandleShillConnectFailure called with no pending request.",
                  service_path);
    return;
  }
  network_handler::ErrorCallback error_callback = request->error_callback;
  pending_requests_.erase(service_path);
  std::string error;
  if (dbus_error_name == shill::kErrorResultAlreadyConnected) {
    error = kErrorConnected;
  } else if (dbus_error_name == shill::kErrorResultInProgress) {
    error = kErrorConnecting;
  } else {
    NET_LOG_ERROR("Connect Failure, Shill error: " + dbus_error_name,
                  service_path);
    error = kErrorConnectFailed;
  }
  InvokeConnectErrorCallback(service_path, error_callback, error);
}

void NetworkConnectionHandler::CheckPendingRequest(
    const std::string service_path) {
  ConnectRequest* request = GetPendingRequest(service_path);
  DCHECK(request);
  if (request->connect_state == ConnectRequest::CONNECT_REQUESTED)
    return;  // Request has not started, ignore update
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!network)
    return;  // NetworkState may not be be updated yet.

  if (network->IsConnectingState()) {
    request->connect_state = ConnectRequest::CONNECT_CONNECTING;
    return;
  }
  if (network->IsConnectedState()) {
    if (!request->profile_path.empty()) {
      // If a profile path was specified, set it on a successful connection.
      configuration_handler_->SetNetworkProfile(
          service_path,
          request->profile_path,
          NetworkConfigurationObserver::SOURCE_USER_ACTION,
          base::Bind(&base::DoNothing),
          chromeos::network_handler::ErrorCallback());
    }
    InvokeConnectSuccessCallback(request->service_path,
                                 request->success_callback);
    pending_requests_.erase(service_path);
    return;
  }
  if (network->connection_state() == shill::kStateIdle &&
      request->connect_state != ConnectRequest::CONNECT_CONNECTING) {
    // Connection hasn't started yet, keep waiting.
    return;
  }

  // Network is neither connecting or connected; an error occurred.
  std::string error_name;  // 'Canceled' or 'Failed'
  if (network->connection_state() == shill::kStateIdle &&
      pending_requests_.size() > 1) {
    // Another connect request canceled this one.
    error_name = kErrorConnectCanceled;
  } else {
    error_name = kErrorConnectFailed;
    if (network->connection_state() != shill::kStateFailure) {
      NET_LOG_ERROR("Unexpected State: " + network->connection_state(),
                    service_path);
    }
  }

  network_handler::ErrorCallback error_callback = request->error_callback;
  pending_requests_.erase(service_path);
  InvokeConnectErrorCallback(service_path, error_callback, error_name);
}

void NetworkConnectionHandler::CheckAllPendingRequests() {
  for (std::map<std::string, ConnectRequest>::iterator iter =
           pending_requests_.begin(); iter != pending_requests_.end(); ++iter) {
    CheckPendingRequest(iter->first);
  }
}

// Connect callbacks

void NetworkConnectionHandler::InvokeConnectSuccessCallback(
    const std::string& service_path,
    const base::Closure& success_callback) {
  NET_LOG_EVENT("Connect Request Succeeded", service_path);
  if (!success_callback.is_null())
    success_callback.Run();
  for (auto& observer : observers_)
    observer.ConnectSucceeded(service_path);
}

void NetworkConnectionHandler::ErrorCallbackForPendingRequest(
    const std::string& service_path,
    const std::string& error_name) {
  ConnectRequest* request = GetPendingRequest(service_path);
  if (!request) {
    NET_LOG_ERROR("ErrorCallbackForPendingRequest with no pending request.",
                  service_path);
    return;
  }
  // Remove the entry before invoking the callback in case it triggers a retry.
  network_handler::ErrorCallback error_callback = request->error_callback;
  pending_requests_.erase(service_path);
  InvokeConnectErrorCallback(service_path, error_callback, error_name);
}

void NetworkConnectionHandler::InvokeConnectErrorCallback(
    const std::string& service_path,
    const network_handler::ErrorCallback& error_callback,
    const std::string& error_name) {
  NET_LOG_ERROR("Connect Failure: " + error_name, service_path);
  network_handler::RunErrorCallback(error_callback, service_path, error_name,
                                    "");
  for (auto& observer : observers_)
    observer.ConnectFailed(service_path, error_name);
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
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 kErrorDisconnectFailed, service_path, error_callback));
}

void NetworkConnectionHandler::HandleShillDisconnectSuccess(
    const std::string& service_path,
    const base::Closure& success_callback) {
  NET_LOG_EVENT("Disconnect Request Sent", service_path);
  if (!success_callback.is_null())
    success_callback.Run();
}

}  // namespace chromeos
