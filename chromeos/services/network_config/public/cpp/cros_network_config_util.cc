// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"

namespace chromeos {
namespace network_config {

// This matches logic in NetworkTypePattern and should be kept in sync.
bool NetworkTypeMatchesType(mojom::NetworkType network_type,
                            mojom::NetworkType match_type) {
  switch (match_type) {
    case mojom::NetworkType::kAll:
      return true;
    case mojom::NetworkType::kMobile:
      return network_type == mojom::NetworkType::kCellular ||
             network_type == mojom::NetworkType::kTether ||
             network_type == mojom::NetworkType::kWiMAX;
    case mojom::NetworkType::kWireless:
      return network_type == mojom::NetworkType::kCellular ||
             network_type == mojom::NetworkType::kTether ||
             network_type == mojom::NetworkType::kWiFi ||
             network_type == mojom::NetworkType::kWiMAX;
    case mojom::NetworkType::kCellular:
    case mojom::NetworkType::kEthernet:
    case mojom::NetworkType::kTether:
    case mojom::NetworkType::kVPN:
    case mojom::NetworkType::kWiFi:
    case mojom::NetworkType::kWiMAX:
      return network_type == match_type;
  }
  NOTREACHED();
  return false;
}

bool NetworkStateMatchesType(const mojom::NetworkStateProperties* network,
                             mojom::NetworkType type) {
  return NetworkTypeMatchesType(network->type, type);
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
