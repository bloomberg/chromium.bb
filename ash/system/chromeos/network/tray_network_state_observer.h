// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H

#include <string>

#include "base/compiler_specific.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace ash {
namespace internal {

class TrayNetwork;

class TrayNetworkStateObserver : public chromeos::NetworkStateHandlerObserver {
 public:
  enum WifiState {
    WIFI_DISABLED,
    WIFI_ENABLED,
    WIFI_UNKNOWN,
  };

  explicit TrayNetworkStateObserver(TrayNetwork* tray);

  virtual ~TrayNetworkStateObserver();

  // NetworkStateHandlerObserver overrides.
  virtual void NetworkManagerChanged() OVERRIDE;
  virtual void NetworkListChanged() OVERRIDE;
  virtual void DeviceListChanged() OVERRIDE;
  virtual void DefaultNetworkChanged(
      const chromeos::NetworkState* network) OVERRIDE;
  virtual void NetworkPropertiesUpdated(
      const chromeos::NetworkState* network) OVERRIDE;

 private:
  TrayNetwork* tray_;
  WifiState wifi_state_;  // cache wifi enabled state

  DISALLOW_COPY_AND_ASSIGN(TrayNetworkStateObserver);
};

}  // namespace ash
}  // namespace internal

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H
