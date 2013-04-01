// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/tray_network_state_observer.h"

#include <set>
#include <string>

#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace internal {

TrayNetworkStateObserver::TrayNetworkStateObserver(Delegate* delegate)
    : delegate_(delegate) {
  chromeos::NetworkStateHandler::Get()->AddObserver(this);
}

TrayNetworkStateObserver::~TrayNetworkStateObserver() {
  chromeos::NetworkStateHandler::Get()->RemoveObserver(this);
}

void TrayNetworkStateObserver::NetworkManagerChanged() {
  delegate_->NetworkStateChanged(false);
}

void TrayNetworkStateObserver::NetworkListChanged() {
  delegate_->NetworkStateChanged(true);
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
  if (network == chromeos::NetworkStateHandler::Get()->DefaultNetwork())
    delegate_->NetworkStateChanged(true);
  delegate_->NetworkServiceChanged(network);
}

}  // namespace ash
}  // namespace internal
