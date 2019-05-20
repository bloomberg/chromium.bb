// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H_
#define ASH_SYSTEM_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H_

#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace service_manager {
class Connector;
}

namespace ash {

class TrayNetworkStateObserver
    : public chromeos::network_config::mojom::CrosNetworkConfigObserver {
 public:
  // TrayNetworkStateObserver observes the mojo interface, and in turn has UI
  // observers that only need to be informed when the UI should be refreshed.
  class Observer : public base::CheckedObserver {
   public:
    // The active networks changed or a device enabled state changed.
    virtual void ActiveNetworkStateChanged();

    // The list of networks changed. The frequency of this event is limited.
    virtual void NetworkListChanged();
  };

  explicit TrayNetworkStateObserver(service_manager::Connector* connector);
  ~TrayNetworkStateObserver() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // CrosNetworkConfigObserver
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>)
      override;
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;

 private:
  void BindCrosNetworkConfig(service_manager::Connector* connector);
  void UpdateDeviceEnabledStates();
  void OnGetDeviceStateList(
      std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
          devices);
  void NotifyNetworkListChanged();
  void SendActiveNetworkStateChanged();
  void SendNetworkListChanged();

  chromeos::network_config::mojom::CrosNetworkConfigPtr
      cros_network_config_ptr_;
  mojo::Binding<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_binding_{this};

  base::ObserverList<Observer> observer_list_;

  // Frequency at which to push NetworkListChanged updates. This avoids
  // unnecessarily frequent UI updates (which can be expensive). We set this
  // to 0 for tests to eliminate timing variance.
  int update_frequency_;

  // Timer used to limit the frequency of NetworkListChanged updates.
  base::OneShotTimer timer_;

  // The cached states of whether Wi-Fi and Mobile are enabled. The tray
  // includes expanding network lists of these types, so these cached values
  // are used to determine when to prioritize updating the tray when they
  // change.
  bool wifi_enabled_ = false;
  bool mobile_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(TrayNetworkStateObserver);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H_
