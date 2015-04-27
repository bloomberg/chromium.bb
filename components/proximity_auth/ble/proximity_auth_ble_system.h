// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_BLE_PROXIMITY_AUTH_BLE_SYSTEM_H_
#define COMPONENTS_PROXIMITY_AUTH_BLE_PROXIMITY_AUTH_BLE_SYSTEM_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

#include "components/proximity_auth/ble/bluetooth_low_energy_connection_finder.h"

namespace proximity_auth {

// This is the main entry point to start Proximity Auth over Bluetooth Low
// Energy. This is the underlying system for the Smart Lock features. It will
// discover Bluetooth Low Energy phones and unlock the lock screen if the phone
// passes an authorization and authentication protocol.
class ProximityAuthBleSystem {
 public:
  ProximityAuthBleSystem();
  ~ProximityAuthBleSystem();

 private:
  scoped_ptr<BluetoothLowEnergyConnectionFinder> connection_finder_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthBleSystem);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_BLE_PROXIMITY_AUTH_BLE_SYSTEM_H_
