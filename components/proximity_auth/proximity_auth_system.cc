// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/proximity_auth_system.h"

namespace proximity_auth {

ProximityAuthSystem::ProximityAuthSystem(
    const std::vector<RemoteDevice>& remote_devices)
    : remote_devices_(remote_devices) {
}

ProximityAuthSystem::~ProximityAuthSystem() {
}

const std::vector<RemoteDevice>& ProximityAuthSystem::GetRemoteDevices() {
  return remote_devices_;
}

}  // proximity_auth
