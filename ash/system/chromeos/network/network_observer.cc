// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_observer.h"

#include "chromeos/network/network_state.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

//static
NetworkObserver::NetworkType NetworkObserver::GetNetworkTypeForNetworkState(
    const chromeos::NetworkState* network) {
  if (!network)
    return NETWORK_UNKNOWN;
  const std::string& type = network->type();
  if (type == flimflam::kTypeCellular) {
    const std::string& technology = network->network_technology();
    if (technology == flimflam::kNetworkTechnologyLte ||
        technology == flimflam::kNetworkTechnologyLteAdvanced)
      return NETWORK_CELLULAR_LTE;
    else
      return NETWORK_CELLULAR;
  }
  if (type == flimflam::kTypeEthernet)
     return NETWORK_ETHERNET;
  if (type == flimflam::kTypeWifi)
     return NETWORK_WIFI;
  if (type == flimflam::kTypeBluetooth)
     return NETWORK_BLUETOOTH;
  return NETWORK_UNKNOWN;
}

}  // namespace ash
