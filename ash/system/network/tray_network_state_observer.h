// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H_
#define ASH_SYSTEM_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace service_manager {
class Connector;
}

namespace ash {

// TrayNetworkStateObserver observes the mojo interface and tracks the devices
// and active networks. It has UI observers that are informed when important
// changes occur.
class ASH_EXPORT TrayNetworkStateObserver
    : public chromeos::network_config::mojom::CrosNetworkConfigObserver {
 public:
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

  // Returns DeviceStateProperties for |type| if it exists or null.
  chromeos::network_config::mojom::DeviceStateProperties* GetDevice(
      chromeos::network_config::mojom::NetworkType type);

  // Returns the DeviceStateType for |type| if a device exists or kUnavailable.
  chromeos::network_config::mojom::DeviceStateType GetDeviceState(
      chromeos::network_config::mojom::NetworkType type);

  chromeos::network_config::mojom::NetworkStateProperties* default_network() {
    return default_network_.get();
  }
  chromeos::network_config::mojom::NetworkStateProperties*
  active_non_cellular() {
    return active_non_cellular_.get();
  }
  chromeos::network_config::mojom::NetworkStateProperties* active_cellular() {
    return active_cellular_.get();
  }
  chromeos::network_config::mojom::NetworkStateProperties* active_vpn() {
    return active_vpn_.get();
  }

 private:
  // CrosNetworkConfigObserver
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>)
      override;
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;

  void BindCrosNetworkConfig(service_manager::Connector* connector);
  void GetDeviceStateList();
  void OnGetDeviceStateList(
      std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
          devices);

  void GetActiveNetworks();
  void UpdateActiveNetworks(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

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

  base::flat_map<chromeos::network_config::mojom::NetworkType,
                 chromeos::network_config::mojom::DeviceStatePropertiesPtr>
      devices_;
  chromeos::network_config::mojom::NetworkStatePropertiesPtr default_network_;
  chromeos::network_config::mojom::NetworkStatePropertiesPtr
      active_non_cellular_;
  chromeos::network_config::mojom::NetworkStatePropertiesPtr active_cellular_;
  chromeos::network_config::mojom::NetworkStatePropertiesPtr active_vpn_;

  DISALLOW_COPY_AND_ASSIGN(TrayNetworkStateObserver);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_TRAY_NETWORK_STATE_OBSERVER_H_
