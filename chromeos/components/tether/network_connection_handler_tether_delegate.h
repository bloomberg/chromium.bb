// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_NETWORK_CONNECTION_HANDLER_TETHER_DELEGATE_H_
#define CHROMEOS_COMPONENTS_TETHER_NETWORK_CONNECTION_HANDLER_TETHER_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/network/network_connection_handler.h"

namespace chromeos {

class NetworkConnectionHandler;

namespace tether {

class ActiveHost;
class TetherConnector;
class TetherDisconnector;

// Handles connect/disconnect requests for Tether networks.
class NetworkConnectionHandlerTetherDelegate
    : public NetworkConnectionHandler::TetherDelegate {
 public:
  NetworkConnectionHandlerTetherDelegate(
      NetworkConnectionHandler* network_connection_handler,
      ActiveHost* active_host,
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
  void OnFailedDisconnectionFromPreviousHost(
      const std::string& tether_network_guid,
      const std::string& error_name);

  NetworkConnectionHandler* network_connection_handler_;
  ActiveHost* active_host_;
  TetherConnector* tether_connector_;
  TetherDisconnector* tether_disconnector_;

  base::WeakPtrFactory<NetworkConnectionHandlerTetherDelegate>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionHandlerTetherDelegate);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_NETWORK_CONNECTION_HANDLER_TETHER_DELEGATE_H_
