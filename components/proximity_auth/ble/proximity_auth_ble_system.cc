// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/proximity_auth_ble_system.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "components/proximity_auth/connection.h"
#include "device/bluetooth/bluetooth_device.h"

#include "components/proximity_auth/ble/bluetooth_low_energy_connection_finder.h"

namespace proximity_auth {

const char kSmartLockServiceUUID[] = "b3b7e28e-a000-3e17-bd86-6e97b9e28c11";

void ConnectionCallback(
    scoped_ptr<device::BluetoothGattConnection> connection) {
  VLOG(1) << "Connection established";
}

ProximityAuthBleSystem::ProximityAuthBleSystem() {
  VLOG(1) << "Starting Proximity Auth over Bluetooth Low Energy.";
  connection_finder_ = scoped_ptr<BluetoothLowEnergyConnectionFinder>(
      new BluetoothLowEnergyConnectionFinder(kSmartLockServiceUUID));
  connection_finder_->Find(base::Bind(&ConnectionCallback));
}

ProximityAuthBleSystem::~ProximityAuthBleSystem() {
  VLOG(1) << "Stopping Proximity over Bluetooth Low Energy.";
  connection_finder_.reset();
}

}  // namespace proximity_auth
