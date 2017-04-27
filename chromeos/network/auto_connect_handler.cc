// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/auto_connect_handler.h"


#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_type_pattern.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

AutoConnectHandler::AutoConnectHandler()
    : client_cert_resolver_(nullptr),
      request_best_connection_pending_(false),
      device_policy_applied_(false),
      user_policy_applied_(false),
      client_certs_resolved_(false),
      applied_autoconnect_policy_(false),
      connect_to_best_services_after_scan_(false),
      weak_ptr_factory_(this) {
}

AutoConnectHandler::~AutoConnectHandler() {
  if (LoginState::IsInitialized())
    LoginState::Get()->RemoveObserver(this);
  if (client_cert_resolver_)
    client_cert_resolver_->RemoveObserver(this);
  if (network_connection_handler_)
    network_connection_handler_->RemoveObserver(this);
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  if (managed_configuration_handler_)
    managed_configuration_handler_->RemoveObserver(this);
}

void AutoConnectHandler::Init(
    ClientCertResolver* client_cert_resolver,
    NetworkConnectionHandler* network_connection_handler,
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_configuration_handler) {
  if (LoginState::IsInitialized())
    LoginState::Get()->AddObserver(this);

  client_cert_resolver_ = client_cert_resolver;
  if (client_cert_resolver_)
    client_cert_resolver_->AddObserver(this);

  network_connection_handler_ = network_connection_handler;
  if (network_connection_handler_)
    network_connection_handler_->AddObserver(this);

  network_state_handler_ = network_state_handler;
  if (network_state_handler_)
    network_state_handler_->AddObserver(this, FROM_HERE);

  managed_configuration_handler_ = managed_network_configuration_handler;
  if (managed_configuration_handler_)
    managed_configuration_handler_->AddObserver(this);

  if (LoginState::IsInitialized())
    LoggedInStateChanged();
}

void AutoConnectHandler::LoggedInStateChanged() {
  if (!LoginState::Get()->IsUserLoggedIn())
    return;

  // Disconnect before connecting, to ensure that we do not disconnect a network
  // that we just connected.
  DisconnectIfPolicyRequires();
  NET_LOG_DEBUG("RequestBestConnection", "User logged in");
  RequestBestConnection();
}

void AutoConnectHandler::ConnectToNetworkRequested(
    const std::string& /*service_path*/) {
  // Stop any pending request to connect to the best newtork.
  request_best_connection_pending_ = false;
}

void AutoConnectHandler::PoliciesChanged(const std::string& userhash) {
  // Ignore user policies.
  if (!userhash.empty())
    return;
  DisconnectIfPolicyRequires();
}

void AutoConnectHandler::PoliciesApplied(const std::string& userhash) {
  if (userhash.empty()) {
    device_policy_applied_ = true;
  } else {
    user_policy_applied_ = true;
    DisconnectIfPolicyRequires();
  }

  // Request to connect to the best network only if there is at least one
  // managed network. Otherwise only process existing requests.
  const ManagedNetworkConfigurationHandler::GuidToPolicyMap* managed_networks =
      managed_configuration_handler_->GetNetworkConfigsFromPolicy(userhash);
  DCHECK(managed_networks);
  if (!managed_networks->empty()) {
    NET_LOG_DEBUG("RequestBestConnection", "Policy applied");
    RequestBestConnection();
  } else {
    CheckBestConnection();
  }
}

void AutoConnectHandler::ScanCompleted(const DeviceState* device) {
  if (!connect_to_best_services_after_scan_ ||
      device->type() != shill::kTypeWifi) {
    return;
  }
  connect_to_best_services_after_scan_ = false;
  // Request ConnectToBestServices after processing any pending DBus calls.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AutoConnectHandler::CallShillConnectToBestServices,
                            weak_ptr_factory_.GetWeakPtr()));
}

void AutoConnectHandler::ResolveRequestCompleted(
    bool network_properties_changed) {
  client_certs_resolved_ = true;

  // Only request to connect to the best network if network properties were
  // actually changed. Otherwise only process existing requests.
  if (network_properties_changed) {
    NET_LOG_DEBUG("RequestBestConnection",
                  "Client certificate patterns resolved");
    RequestBestConnection();
  } else {
    CheckBestConnection();
  }
}

void AutoConnectHandler::RequestBestConnection() {
  request_best_connection_pending_ = true;
  CheckBestConnection();
}

void AutoConnectHandler::CheckBestConnection() {
  // Return immediately if there is currently no request pending to change to
  // the best network.
  if (!request_best_connection_pending_)
    return;

  bool policy_application_running =
      managed_configuration_handler_->IsAnyPolicyApplicationRunning();
  bool client_cert_resolve_task_running =
      client_cert_resolver_->IsAnyResolveTaskRunning();
  VLOG(2) << "device policy applied: " << device_policy_applied_
          << "\nuser policy applied: " << user_policy_applied_
          << "\npolicy application running: " << policy_application_running
          << "\nclient cert patterns resolved: " << client_certs_resolved_
          << "\nclient cert resolve task running: "
          << client_cert_resolve_task_running;
  if (!device_policy_applied_ || policy_application_running ||
      client_cert_resolve_task_running) {
    return;
  }

  if (LoginState::Get()->IsUserLoggedIn()) {
    // Before changing connection after login, we wait at least for:
    //  - user policy applied at least once
    //  - client certificate patterns resolved
    if (!user_policy_applied_ || !client_certs_resolved_)
      return;
  }

  request_best_connection_pending_ = false;

  // Trigger a ConnectToBestNetwork request after the next scan completion.
  // Note: there is an edge case here if a scan is in progress and a hidden
  // network has been configured since the scan started. crbug.com/433075.
  if (connect_to_best_services_after_scan_)
    return;
  connect_to_best_services_after_scan_ = true;
  if (!network_state_handler_->GetScanningByType(
          NetworkTypePattern::Primitive(shill::kTypeWifi))) {
    network_state_handler_->RequestScan();
  }
}

void AutoConnectHandler::DisconnectIfPolicyRequires() {
  if (applied_autoconnect_policy_ || !LoginState::Get()->IsUserLoggedIn() ||
      !user_policy_applied_) {
    return;
  }

  const base::DictionaryValue* global_network_config =
      managed_configuration_handler_->GetGlobalConfigFromPolicy(
          std::string() /* no username hash, device policy */);

  if (!global_network_config)
    return;  // Device policy is not set, yet.

  applied_autoconnect_policy_ = true;

  bool only_policy_autoconnect = false;
  global_network_config->GetBooleanWithoutPathExpansion(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToAutoconnect,
      &only_policy_autoconnect);
  bool only_policy_connect = false;
  global_network_config->GetBooleanWithoutPathExpansion(
      ::onc::global_network_config::kAllowOnlyPolicyNetworksToConnect,
      &only_policy_connect);

  if (only_policy_autoconnect || only_policy_connect)
    DisconnectFromUnmanagedSharedWiFiNetworks();
}

void AutoConnectHandler::DisconnectFromUnmanagedSharedWiFiNetworks() {
  NET_LOG_DEBUG("DisconnectFromUnmanagedSharedWiFiNetworks", "");

  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Wireless(), &networks);
  for (const NetworkState* network : networks) {
    if (!(network->IsConnectingState() || network->IsConnectedState()))
      break;  // Connected and connecting networks are listed first.

    if (network->IsPrivate())
      continue;

    const bool network_is_policy_managed =
        !network->profile_path().empty() && !network->guid().empty() &&
        managed_configuration_handler_->FindPolicyByGuidAndProfile(
            network->guid(), network->profile_path(), nullptr /* onc_source */);
    if (network_is_policy_managed)
      continue;

    NET_LOG_EVENT("Disconnect Forced by Policy", network->path());
    DBusThreadManager::Get()->GetShillServiceClient()->Disconnect(
        dbus::ObjectPath(network->path()), base::Bind(&base::DoNothing),
        base::Bind(&network_handler::ShillErrorCallbackFunction,
                   "AutoConnectHandler.Disconnect failed", network->path(),
                   network_handler::ErrorCallback()));
  }
}

void AutoConnectHandler::CallShillConnectToBestServices() const {
  NET_LOG_EVENT("ConnectToBestServices", "");
  DBusThreadManager::Get()->GetShillManagerClient()->ConnectToBestServices(
      base::Bind(&base::DoNothing),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 "ConnectToBestServices Failed",
                 "", network_handler::ErrorCallback()));
}

}  // namespace chromeos
