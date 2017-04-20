// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/tether_device_state_manager.h"

#include "chromeos/network/network_state_handler.h"

namespace chromeos {

namespace tether {

TetherDeviceStateManager::TetherDeviceStateManager(
    NetworkStateHandler* network_state_handler)
    : network_state_handler_(network_state_handler) {
  network_state_handler_->AddObserver(this, FROM_HERE);

  // TODO(hansberry): Set the appropriate value. For now, always set ENABLED.
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);
}

TetherDeviceStateManager::~TetherDeviceStateManager() {
  network_state_handler_->RemoveObserver(this, FROM_HERE);

  // TODO(hansberry): Determine if this should be PROHIBITED or UNAVAILABLE
  // based on reason for shutting down.
  network_state_handler_->SetTetherTechnologyState(
      NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE);
}

void TetherDeviceStateManager::DeviceListChanged() {
  // TODO(hansberry): Implement.
}

}  // namespace tether

}  // namespace chromeos
