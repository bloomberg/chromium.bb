// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_NETWORK_CONNECTION_HANDLER_TETHER_DELEGATE_H_
#define CHROMEOS_COMPONENTS_TETHER_NETWORK_CONNECTION_HANDLER_TETHER_DELEGATE_H_

#include "base/macros.h"
#include "chromeos/network/network_connection_handler.h"

namespace chromeos {

class NetworkConnectionHandler;

namespace tether {

class TetherConnector;
class TetherDisconnector;

// Handles connect/disconnect requests for Tether networks.
class NetworkConnectionHandlerTetherDelegate
    : public NetworkConnectionHandler::TetherDelegate {
 public:
  NetworkConnectionHandlerTetherDelegate(
      NetworkConnectionHandler* network_connection_handler,
      TetherConnector* tether_connector,
      TetherDisconnector* tether_disconnector);
  ~NetworkConnectionHandlerTetherDelegate() override;

  // NetworkConnectionHandler::TetherDelegate:
  void DisconnectFromNetwork(
      const std::string& tether_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback) override;
  void ConnectToNetwork(
      const std::string& tether_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback) override;

 private:
  NetworkConnectionHandler* network_connection_handler_;
  TetherConnector* tether_connector_;
  TetherDisconnector* tether_disconnector_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionHandlerTetherDelegate);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_NETWORK_CONNECTION_HANDLER_TETHER_DELEGATE_H_
