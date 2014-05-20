// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H

#include <string>

#include "base/compiler_specific.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace ash {

class TrayNetworkStateObserver : public chromeos::NetworkStateHandlerObserver {
 public:
  class Delegate {
   public:
    // Called when the network state may have changed. If |list_changed| is
    // true then the list of networks may have changed.
    virtual void NetworkStateChanged(bool list_changed) = 0;

    // Called when the properties for |network| may have been updated.
    virtual void NetworkServiceChanged(
        const chromeos::NetworkState* network) = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit TrayNetworkStateObserver(Delegate* delegate);

  virtual ~TrayNetworkStateObserver();

  // NetworkStateHandlerObserver overrides.
  virtual void NetworkListChanged() OVERRIDE;
  virtual void DeviceListChanged() OVERRIDE;
  virtual void DefaultNetworkChanged(
      const chromeos::NetworkState* network) OVERRIDE;
  virtual void NetworkConnectionStateChanged(
      const chromeos::NetworkState* network) OVERRIDE;
  virtual void NetworkPropertiesUpdated(
      const chromeos::NetworkState* network) OVERRIDE;

 private:
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TrayNetworkStateObserver);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H
