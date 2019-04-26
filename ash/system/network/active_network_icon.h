// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_ACTIVE_NETWORK_ICON_H_
#define ASH_SYSTEM_NETWORK_ACTIVE_NETWORK_ICON_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/network/network_icon.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace service_manager {
class Connector;
}

namespace ash {

// Tracks changes to the active networks and provides an interface to
// network_icon for the default network. This class supports two interfaces:
// * Single: A single icon is shown to represent the active network state.
// * Dual: One or two icons are shown to represent the active network state:
// ** Primary: The state of the primary active network. If Cellular, a
//    a technology badge is used to represent the network.
// ** Cellular (enabled devices only): The state of the Cellular connection if
//    available regardless of whether it is the active network.
// NOTE : GetSingleDefaultImage and GetDefaultLabel are partially tested in
// network_icon_unittest.cc, and partially in active_network_icon_unittest.cc.
// TODO(stevenjb): Move all test coverage to active_network_icon_unittest.cc and
// test Dual icon methods.
class ASH_EXPORT ActiveNetworkIcon
    : public chromeos::network_config::mojom::CrosNetworkConfigObserver {
 public:
  explicit ActiveNetworkIcon(service_manager::Connector* connector);
  ~ActiveNetworkIcon() override;

  // Returns the label for the primary active network..
  base::string16 GetDefaultLabel(network_icon::IconType icon_type);

  // Single image mode. Returns a network icon (which may be empty) and sets
  // |animating| if provided.
  gfx::ImageSkia GetSingleImage(network_icon::IconType icon_type,
                                bool* animating);

  // Dual image mode. Returns the primary icon (which may be empty) and sets
  // |animating| if provided.
  gfx::ImageSkia GetDualImagePrimary(network_icon::IconType icon_type,
                                     bool* animating);

  // Dual image mode. Returns the Cellular icon (which may be empty) and sets
  // |animating| if provided.
  gfx::ImageSkia GetDualImageCellular(network_icon::IconType icon_type,
                                      bool* animating);

 private:
  void BindCrosNetworkConfig(service_manager::Connector* connector);
  gfx::ImageSkia GetDefaultImageImpl(
      const base::Optional<network_icon::NetworkIconState>& default_network,
      network_icon::IconType icon_type,
      bool* animating);

  // Called when there is no default network., Provides an empty or disabled
  // wifi icon and sets |animating| if provided to false.
  gfx::ImageSkia GetDefaultImageForNoNetwork(network_icon::IconType icon_type,
                                             bool* animating);

  void UpdateActiveNetworks();
  void SetCellularUninitializedMsg();

  // CrosNetworkConfigObserver
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) override;
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;

  void OnGetDeviceStateList(
      std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
          devices);
  chromeos::network_config::mojom::DeviceStateProperties* GetDevice(
      chromeos::network_config::mojom::NetworkType type);

  chromeos::network_config::mojom::CrosNetworkConfigPtr
      cros_network_config_ptr_;
  mojo::Binding<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_binding_{this};

  base::flat_map<chromeos::network_config::mojom::NetworkType,
                 chromeos::network_config::mojom::DeviceStatePropertiesPtr>
      devices_;
  base::Optional<network_icon::NetworkIconState> default_network_;
  base::Optional<network_icon::NetworkIconState> active_non_cellular_;
  base::Optional<network_icon::NetworkIconState> active_cellular_;
  base::Optional<network_icon::NetworkIconState> active_vpn_;
  int cellular_uninitialized_msg_ = 0;
  base::Time uninitialized_state_time_;

  DISALLOW_COPY_AND_ASSIGN(ActiveNetworkIcon);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_ACTIVE_NETWORK_ICON_H_
