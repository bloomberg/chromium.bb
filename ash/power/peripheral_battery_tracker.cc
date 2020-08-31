// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/peripheral_battery_tracker.h"

#include "ash/power/gatt_battery_controller.h"
#include "ash/power/hfp_battery_listener.h"
#include "ash/power/hid_battery_listener.h"
#include "base/bind.h"
#include "base/check.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash {

PeripheralBatteryTracker::PeripheralBatteryTracker() {
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&PeripheralBatteryTracker::InitializeOnBluetoothReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

PeripheralBatteryTracker::~PeripheralBatteryTracker() = default;

void PeripheralBatteryTracker::InitializeOnBluetoothReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  DCHECK(adapter_.get());
  gatt_battery_controller_ = std::make_unique<GattBatteryController>(adapter_);
  hfp_battery_listener_ = std::make_unique<HfpBatteryListener>(adapter_);
  hid_battery_listener_ = std::make_unique<HidBatteryListener>(adapter_);
}

}  // namespace ash
