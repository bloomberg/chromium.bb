// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_DEVICE_STATE_MANAGER_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_DEVICE_STATE_MANAGER_H_

#include "base/macros.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkStateHandler;

namespace tether {

// Manages the device state of tether networks. The tether state is:
//   *TECHNOLOGY_PROHIBITED when tethering is disallowed by policy.
//   *TECHNOLOGY_UNAVAILABLE when tethering is allowed by policy, but either
//    the device has no Bluetooth capabilities or the logged-in user has no
//    tether hosts associated with their account.
//   *TECHNOLOGY_AVAILABLE when tethering is allowed by policy, the device has
//    Bluetooth capabilities, the logged-in user has tether hosts associated
//    with their account, but the Instant Tethering setting is disabled.
//   *TECHNOLOGY_ENABLED when tethering is allowed by policy, the device has
//    Bluetooth capabilities, the logged-in user has tether hosts associated
//    with their account, and the Instant Tethering setting is enabled.
// TODO(hansberry): Complete this class description when implemented.
class TetherDeviceStateManager : public NetworkStateHandlerObserver {
 public:
  explicit TetherDeviceStateManager(NetworkStateHandler* network_state_handler);
  ~TetherDeviceStateManager() override;

  // NetworkStateHandlerObserver:
  void DeviceListChanged() override;

 private:
  NetworkStateHandler* network_state_handler_;

  DISALLOW_COPY_AND_ASSIGN(TetherDeviceStateManager);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_DEVICE_STATE_MANAGER_H_
