// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"

namespace chromeos {
namespace network_config {

bool NetworkStateMatchesType(const mojom::NetworkStateProperties* network,
                             mojom::NetworkType type) {
  switch (type) {
    case mojom::NetworkType::kAll:
      return true;
    case mojom::NetworkType::kCellular:
    case mojom::NetworkType::kEthernet:
    case mojom::NetworkType::kTether:
    case mojom::NetworkType::kVPN:
    case mojom::NetworkType::kWiFi:
    case mojom::NetworkType::kWiMAX:
      return network->type == type;
    case mojom::NetworkType::kWireless:
      return network->type == mojom::NetworkType::kCellular ||
             network->type == mojom::NetworkType::kTether ||
             network->type == mojom::NetworkType::kWiFi ||
             network->type == mojom::NetworkType::kWiMAX;
  }
  NOTREACHED();
  return false;
}

bool StateIsConnected(mojom::ConnectionStateType connection_state) {
  switch (connection_state) {
    case mojom::ConnectionStateType::kOnline:
    case mojom::ConnectionStateType::kConnected:
    case mojom::ConnectionStateType::kPortal:
      return true;
    case mojom::ConnectionStateType::kConnecting:
    case mojom::ConnectionStateType::kNotConnected:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace network_config
}  // namespace chromeos
