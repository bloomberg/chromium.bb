// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_NETWORK_DISCONNECTION_HANDLER_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_NETWORK_DISCONNECTION_HANDLER_H_

#include "base/macros.h"
#include "chromeos/components/tether/active_host.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkStateHandler;

namespace tether {

class ActiveHost;

// Handles lost Wi-Fi connections for ongoing tether sessions. When a tether
// connection is in progress, the device is connected to an underlying Wi-Fi
// network. If the Wi-Fi connection is disrupted (e.g., by the tether host going
// out of Wi-Fi range), tether metadata must be updated accordingly. This class
// tracks ongoing connections and updates this metadata when necessary.
class TetherNetworkDisconnectionHandler : public NetworkStateHandlerObserver {
 public:
  TetherNetworkDisconnectionHandler(ActiveHost* active_host);
  ~TetherNetworkDisconnectionHandler() override;

  // NetworkStateHandlerObserver:
  void NetworkConnectionStateChanged(const NetworkState* network) override;

 private:
  friend class TetherNetworkDisconnectionHandlerTest;

  TetherNetworkDisconnectionHandler(ActiveHost* active_host,
                                    NetworkStateHandler* network_state_handler);

  ActiveHost* active_host_;
  NetworkStateHandler* network_state_handler_;

  DISALLOW_COPY_AND_ASSIGN(TetherNetworkDisconnectionHandler);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_NETWORK_DISCONNECTION_HANDLER_H_
