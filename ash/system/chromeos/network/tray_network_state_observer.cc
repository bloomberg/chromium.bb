// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/tray_network_state_observer.h"

#include <set>
#include <string>

#include "ash/system/chromeos/network/network_icon.h"
#include "base/location.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

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
  network_icon::PurgeNetworkIconCache();
}

void TrayNetworkStateObserver::DeviceListChanged() {
  delegate_->NetworkStateChanged(false);
}

void TrayNetworkStateObserver::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  delegate_->NetworkStateChanged(true);
}

void TrayNetworkStateObserver::NetworkPropertiesUpdated(
    const chromeos::NetworkState* network) {
  if (network ==
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork())
    delegate_->NetworkStateChanged(true);
  delegate_->NetworkServiceChanged(network);
}

}  // namespace ash
