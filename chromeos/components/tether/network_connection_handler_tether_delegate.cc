// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/network_connection_handler_tether_delegate.h"

#include "chromeos/components/tether/tether_connector.h"
#include "chromeos/components/tether/tether_disconnector.h"

namespace chromeos {

namespace tether {

NetworkConnectionHandlerTetherDelegate::NetworkConnectionHandlerTetherDelegate(
    NetworkConnectionHandler* network_connection_handler,
    TetherConnector* tether_connector,
    TetherDisconnector* tether_disconnector)
    : network_connection_handler_(network_connection_handler),
      tether_connector_(tether_connector),
      tether_disconnector_(tether_disconnector) {
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
  tether_connector_->ConnectToNetwork(tether_network_guid, success_callback,
                                      error_callback);
}

}  // namespace tether

}  // namespace chromeos
