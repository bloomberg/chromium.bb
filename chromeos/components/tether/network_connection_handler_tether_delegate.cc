// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/network_connection_handler_tether_delegate.h"

#include "base/bind_helpers.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/components/tether/tether_connector.h"
#include "chromeos/components/tether/tether_disconnector.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

NetworkConnectionHandlerTetherDelegate::NetworkConnectionHandlerTetherDelegate(
    NetworkConnectionHandler* network_connection_handler,
    ActiveHost* active_host,
    TetherConnector* tether_connector,
    TetherDisconnector* tether_disconnector)
    : network_connection_handler_(network_connection_handler),
      active_host_(active_host),
      tether_connector_(tether_connector),
      tether_disconnector_(tether_disconnector),
      weak_ptr_factory_(this) {
  network_connection_handler_->SetTetherDelegate(this);
}

NetworkConnectionHandlerTetherDelegate::
    ~NetworkConnectionHandlerTetherDelegate() {
  network_connection_handler_->SetTetherDelegate(nullptr);
}

void NetworkConnectionHandlerTetherDelegate::DisconnectFromNetwork(
    const std::string& tether_network_guid,
    const base::Closure& success_callback,
    const network_handler::StringResultCallback& error_callback) {
  tether_disconnector_->DisconnectFromNetwork(tether_network_guid,
                                              success_callback, error_callback);
}

void NetworkConnectionHandlerTetherDelegate::ConnectToNetwork(
    const std::string& tether_network_guid,
    const base::Closure& success_callback,
    const network_handler::StringResultCallback& error_callback) {
  if (active_host_->GetActiveHostStatus() ==
      ActiveHost::ActiveHostStatus::CONNECTED) {
    if (active_host_->GetTetherNetworkGuid() == tether_network_guid) {
      PA_LOG(WARNING) << "Received a request to connect to Tether network with "
                      << "GUID " << tether_network_guid << ", but that network "
                      << "is already connected. Ignoring this request.";
      error_callback.Run(NetworkConnectionHandler::kErrorConnected);
      return;
    }

    std::string previous_host_guid = active_host_->GetTetherNetworkGuid();
    DCHECK(!previous_host_guid.empty());

    PA_LOG(INFO) << "Connection requested to GUID " << tether_network_guid
                 << ", but there is already an active connection. "
                 << "Disconnecting from network with GUID "
                 << previous_host_guid << ".";
    DisconnectFromNetwork(
        previous_host_guid, base::Bind(&base::DoNothing),
        base::Bind(&NetworkConnectionHandlerTetherDelegate::
                       OnFailedDisconnectionFromPreviousHost,
                   weak_ptr_factory_.GetWeakPtr(), previous_host_guid));
  }

  tether_connector_->ConnectToNetwork(tether_network_guid, success_callback,
                                      error_callback);
}

void NetworkConnectionHandlerTetherDelegate::
    OnFailedDisconnectionFromPreviousHost(
        const std::string& tether_network_guid,
        const std::string& error_name) {
  PA_LOG(ERROR) << "Failed to disconnect from previously-active host. "
                << "GUID: " << tether_network_guid << ", Error: " << error_name;
}

}  // namespace tether

}  // namespace chromeos
