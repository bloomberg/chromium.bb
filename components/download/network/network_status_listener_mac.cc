// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/network/network_status_listener_mac.h"

namespace download {

NetworkStatusListenerMac::NetworkStatusListenerMac() = default;

NetworkStatusListenerMac::~NetworkStatusListenerMac() = default;

void NetworkStatusListenerMac::Start(
    NetworkStatusListener::Observer* observer) {}

void NetworkStatusListenerMac::Stop() {}

network::mojom::ConnectionType NetworkStatusListenerMac::GetConnectionType() {
  return network::mojom::ConnectionType::CONNECTION_WIFI;
}

}  // namespace download
