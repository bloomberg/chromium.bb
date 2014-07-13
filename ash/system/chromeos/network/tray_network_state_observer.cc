// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/tray_network_state_observer.h"

#include <set>
#include <string>

#include "base/location.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/chromeos/network/network_icon.h"

using chromeos::NetworkHandler;

namespace ash {

TrayNetworkStateObserver::TrayNetworkStateObserver(Delegate* delegate)
    : delegate_(delegate) {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->AddObserver(
        this, FROM_HERE);
  }
}

TrayNetworkStateObserver::~TrayNetworkStateObserver() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
}

void TrayNetworkStateObserver::NetworkListChanged() {
  delegate_->NetworkStateChanged(true);
  ui::network_icon::PurgeNetworkIconCache();
}

void TrayNetworkStateObserver::DeviceListChanged() {
  delegate_->NetworkStateChanged(false);
}

// Any change to the Default (primary connected) network, including Strength
// changes, should trigger a NetworkStateChanged update.
void TrayNetworkStateObserver::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  delegate_->NetworkStateChanged(true);
}

// Any change to the Connection State should trigger a NetworkStateChanged
// update. This is important when both a VPN and a physical network are
// connected.
void TrayNetworkStateObserver::NetworkConnectionStateChanged(
    const chromeos::NetworkState* network) {
  delegate_->NetworkStateChanged(true);
}

// This tracks Strength and other property changes for all networks. It will
// be called in addition to NetworkConnectionStateChanged for connection state
// changes.
void TrayNetworkStateObserver::NetworkPropertiesUpdated(
    const chromeos::NetworkState* network) {
  if (network ==
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork()) {
    // Trigger NetworkStateChanged in case the Strength property of the
    // Default network changed.
    delegate_->NetworkStateChanged(true);
  }
  delegate_->NetworkServiceChanged(network);
}

}  // namespace ash
