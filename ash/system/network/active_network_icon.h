// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_ACTIVE_NETWORK_ICON_H_
#define ASH_SYSTEM_NETWORK_ACTIVE_NETWORK_ICON_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/network/network_icon.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {
class NetworkState;
class NetworkStateHandler;
}  // namespace chromeos

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// Tracks changes to the active networks and provides an interface to
// network_icon for the default network. NOTE: This class is tested in
// network_icon_unittest.cc.
class ASH_EXPORT ActiveNetworkIcon
    : public chromeos::NetworkStateHandlerObserver {
 public:
  ActiveNetworkIcon();
  ~ActiveNetworkIcon() override;

  // Returns the default image and sets |animating| if provided.
  gfx::ImageSkia GetDefaultImage(network_icon::IconType icon_type,
                                 bool* animating);

  // Returns the label for the default image.
  base::string16 GetDefaultLabel(network_icon::IconType icon_type);

 private:
  // Returns the default image and sets |animating| if provided when there is
  // no default network.
  gfx::ImageSkia GetDefaultImageForNoNetwork(network_icon::IconType icon_type,
                                             bool* animating);

  void UpdateActiveNetworks();

  // chromeos::NetworkStateHandlerObserver
  void DeviceListChanged() override;
  void ActiveNetworksChanged(const std::vector<const chromeos::NetworkState*>&
                                 active_networks) override;
  void OnShuttingDown() override;

  chromeos::NetworkStateHandler* network_state_handler_ = nullptr;
  const chromeos::NetworkState* default_network_ = nullptr;
  const chromeos::NetworkState* active_vpn_ = nullptr;
  int cellular_uninitialized_msg_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ActiveNetworkIcon);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_ACTIVE_NETWORK_ICON_H_
