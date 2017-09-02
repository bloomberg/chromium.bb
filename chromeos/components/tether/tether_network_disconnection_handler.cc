// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_network_disconnection_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/tether/disconnect_tethering_request_sender.h"
#include "chromeos/components/tether/network_configuration_remover.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

TetherNetworkDisconnectionHandler::TetherNetworkDisconnectionHandler(
    ActiveHost* active_host,
    NetworkStateHandler* network_state_handler,
    NetworkConfigurationRemover* network_configuration_remover,
    DisconnectTetheringRequestSender* disconnect_tethering_request_sender)
    : active_host_(active_host),
      network_state_handler_(network_state_handler),
      network_configuration_remover_(network_configuration_remover),
      disconnect_tethering_request_sender_(
          disconnect_tethering_request_sender) {
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

    network_configuration_remover_->RemoveNetworkConfiguration(
        active_host_->GetWifiNetworkGuid());

    // Send a DisconnectTetheringRequest to the tether host so that it can shut
    // down its Wi-Fi hotspot if it is no longer in use.
    disconnect_tethering_request_sender_->SendDisconnectRequestToDevice(
        active_host_->GetActiveHostDeviceId());

    active_host_->SetActiveHostDisconnected();
  }
}

}  // namespace tether

}  // namespace chromeos
