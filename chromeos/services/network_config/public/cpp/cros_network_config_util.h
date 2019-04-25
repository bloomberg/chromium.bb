// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_UTIL_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_UTIL_H_

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace chromeos {
namespace network_config {

// Returns true if network->type matches |type|, which may include kAll or
// kWireless.
bool NetworkStateMatchesType(const mojom::NetworkStateProperties* network,
                             mojom::NetworkType type);

// Returns true if |connection_state| is in a connected state, including portal.
bool StateIsConnected(mojom::ConnectionStateType connection_state);

}  // namespace network_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_UTIL_H_
