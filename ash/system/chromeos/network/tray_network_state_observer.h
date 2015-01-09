// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H

#include <string>

#include "base/compiler_specific.h"
#include "base/timer/timer.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace ash {

class TrayNetworkStateObserver : public chromeos::NetworkStateHandlerObserver {
 public:
  class Delegate {
   public:
    // Called when any interesting network changes occur. The frequency of this
    // event is limited to kUpdateFrequencyMs.
    virtual void NetworkStateChanged() = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit TrayNetworkStateObserver(Delegate* delegate);

  ~TrayNetworkStateObserver() override;

  // NetworkStateHandlerObserver overrides.
  void NetworkListChanged() override;
  void DeviceListChanged() override;
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;
  void NetworkConnectionStateChanged(
      const chromeos::NetworkState* network) override;
  void NetworkPropertiesUpdated(const chromeos::NetworkState* network) override;

 private:
  void SignalUpdate();
  void SendNetworkStateChanged();

  // Unowned Delegate pointer (must outlive this instance).
  Delegate* delegate_;

  // Set to true when we should purge stale icons in the cache.
  bool purge_icons_;

  // Frequency at which to push NetworkStateChanged updates. This avoids
  // unnecessarily frequent UI updates (which can be expensive). We set this
  // to 0 for tests to eliminate timing variance.
  int update_frequency_;

  // Timer used to limit the frequency of NetworkStateChanged updates.
  base::OneShotTimer<TrayNetworkStateObserver> timer_;

  DISALLOW_COPY_AND_ASSIGN(TrayNetworkStateObserver);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H
