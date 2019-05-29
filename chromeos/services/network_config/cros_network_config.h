// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_CROS_NETWORK_CONFIG_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_CROS_NETWORK_CONFIG_H_

#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace chromeos {

class NetworkStateHandler;

namespace network_config {

class CrosNetworkConfig : public mojom::CrosNetworkConfig,
                          public NetworkStateHandlerObserver {
 public:
  explicit CrosNetworkConfig(NetworkStateHandler* network_state_handler);
  ~CrosNetworkConfig() override;

  void BindRequest(mojom::CrosNetworkConfigRequest request);

  // mojom::CrosNetworkConfig
  void AddObserver(mojom::CrosNetworkConfigObserverPtr observer) override;
  void GetNetworkState(const std::string& guid,
                       GetNetworkStateCallback callback) override;
  void GetNetworkStateList(mojom::NetworkFilterPtr filter,
                           GetNetworkStateListCallback callback) override;
  void GetDeviceStateList(GetDeviceStateListCallback callback) override;
  void SetNetworkTypeEnabledState(
      mojom::NetworkType type,
      bool enabled,
      SetNetworkTypeEnabledStateCallback callback) override;
  void RequestNetworkScan(mojom::NetworkType type) override;

  // NetworkStateHandlerObserver
  void NetworkListChanged() override;
  void DeviceListChanged() override;
  void ActiveNetworksChanged(
      const std::vector<const NetworkState*>& active_networks) override;
  void DevicePropertiesUpdated(const DeviceState* device) override;
  void OnShuttingDown() override;

 private:
  mojom::NetworkStatePropertiesPtr GetMojoNetworkState(
      const NetworkState* network);

  NetworkStateHandler* network_state_handler_;  // Unowned
  mojo::InterfacePtrSet<mojom::CrosNetworkConfigObserver> observers_;
  mojo::BindingSet<mojom::CrosNetworkConfig> bindings_;

  DISALLOW_COPY_AND_ASSIGN(CrosNetworkConfig);
};

}  // namespace network_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_CROS_NETWORK_CONFIG_H_
