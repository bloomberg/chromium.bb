// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/proximity_auth_ble_system.h"

#include "base/logging.h"

namespace proximity_auth {

ProximityAuthBleSystem::ProximityAuthBleSystem() {
  VLOG(1) << "Starting Proximity Auth over Bluetooth Low Energy.";
}

ProximityAuthBleSystem::~ProximityAuthBleSystem() {
  VLOG(1) << "Stopping Proximity over Bluetooth Low Energy.";
}

}  // namespace proximity_auth
