// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_network_disconnection_handler.h"

#include "base/memory/ptr_util.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

TetherNetworkDisconnectionHandler::TetherNetworkDisconnectionHandler(
    ActiveHost* active_host)
    : TetherNetworkDisconnectionHandler(
          active_host,
          NetworkHandler::Get()->network_state_handler()) {}

TetherNetworkDisconnectionHandler::TetherNetworkDisconnectionHandler(
    ActiveHost* active_host,
    NetworkStateHandler* network_state_handler)
    : active_host_(active_host), network_state_handler_(network_state_handler) {
  network_state_handler_->AddObserver(this, FROM_HERE);
}

TetherNetworkDisconnectionHandler::~TetherNetworkDisconnectionHandler() {
  network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void TetherNetworkDisconnectionHandler::NetworkConnectionStateChanged(
    const NetworkState* network) {
  // Note: |active_host_->GetWifiNetworkGuid()| returns "" unless currently
  // connected, so this if() statement is only entered on disconnections.
  if (network->guid() == active_host_->GetWifiNetworkGuid() &&
      !network->IsConnectedState()) {
    PA_LOG(INFO) << "Connection to active host (Wi-Fi network GUID "
                 << network->guid() << ") has been lost.";
    active_host_->SetActiveHostDisconnected();

    // TODO(hansberry): Remove Wi-Fi network from "known" networks.
  }
}

}  // namespace tether

}  // namespace chromeos
