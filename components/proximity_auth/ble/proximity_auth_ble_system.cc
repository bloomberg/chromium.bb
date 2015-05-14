// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/proximity_auth_ble_system.h"

#include "base/bind.h"
#include "base/logging.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_connection.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_connection_finder.h"
#include "components/proximity_auth/connection.h"
#include "components/proximity_auth/remote_device.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"

namespace proximity_auth {

namespace {

// The UUID of the Bluetooth Low Energy service.
const char kSmartLockServiceUUID[] = "b3b7e28e-a000-3e17-bd86-6e97b9e28c11";

// The UUID of the characteristic used to send data to the peripheral.
const char kToPeripheralCharUUID[] = "977c6674-1239-4e72-993b-502369b8bb5a";

// The UUID of the characteristic used to receive data from the peripheral.
const char kFromPeripheralCharUUID[] = "f4b904a2-a030-43b3-98a8-221c536c03cb";

}  // namespace

ProximityAuthBleSystem::ProximityAuthBleSystem(
    ScreenlockBridge* screenlock_bridge,
    content::BrowserContext* browser_context)
    : screenlock_bridge_(screenlock_bridge),
      browser_context_(browser_context),
      weak_ptr_factory_(this) {
  DCHECK(screenlock_bridge_);
  DCHECK(browser_context_);
  VLOG(1) << "Starting Proximity Auth over Bluetooth Low Energy.";
  screenlock_bridge_->AddObserver(this);
}

ProximityAuthBleSystem::~ProximityAuthBleSystem() {
  VLOG(1) << "Stopping Proximity over Bluetooth Low Energy.";
  screenlock_bridge_->RemoveObserver(this);
}

void ProximityAuthBleSystem::OnScreenDidLock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  VLOG(1) << "OnScreenDidLock: " << screen_type;
  switch (screen_type) {
    case ScreenlockBridge::LockHandler::SIGNIN_SCREEN:
      connection_finder_.reset();
      break;
    case ScreenlockBridge::LockHandler::LOCK_SCREEN:
      DCHECK(!connection_finder_);
      connection_finder_.reset(new BluetoothLowEnergyConnectionFinder(
          kSmartLockServiceUUID, kToPeripheralCharUUID,
          kFromPeripheralCharUUID));
      connection_finder_->Find(
          base::Bind(&ProximityAuthBleSystem::OnConnectionFound,
                     weak_ptr_factory_.GetWeakPtr()));
      break;
    case ScreenlockBridge::LockHandler::OTHER_SCREEN:
      connection_finder_.reset();
      break;
  }
};

void ProximityAuthBleSystem::OnScreenDidUnlock(
    ScreenlockBridge::LockHandler::ScreenType screen_type) {
  VLOG(1) << "OnScreenDidUnlock: " << screen_type;
  connection_->Disconnect();
  connection_.reset();
  connection_finder_.reset();
};

void ProximityAuthBleSystem::OnFocusedUserChanged(const std::string& user_id) {
  VLOG(1) << "OnFocusedUserChanged: " << user_id;
};

void ProximityAuthBleSystem::OnConnectionFound(
    scoped_ptr<Connection> connection) {
  VLOG(1) << "Connection found. Unlock.";

  connection_ = connection.Pass();

  // Unlock the screen when a connection is found.
  //
  // Note that this magically unlocks Chrome (no user interaction is needed).
  // This user experience for this operation will be greately improved once
  // the Proximity Auth Unlock Manager migration to C++ is done.
  screenlock_bridge_->Unlock(browser_context_);
}

}  // namespace proximity_auth
