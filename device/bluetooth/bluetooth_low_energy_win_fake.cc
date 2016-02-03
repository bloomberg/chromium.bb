// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_win_fake.h"

namespace device {
namespace win {

BluetoothLowEnergyWrapperFake::BluetoothLowEnergyWrapperFake() {}
BluetoothLowEnergyWrapperFake::~BluetoothLowEnergyWrapperFake() {}

bool BluetoothLowEnergyWrapperFake::EnumerateKnownBluetoothLowEnergyDevices(
    ScopedVector<BluetoothLowEnergyDeviceInfo>* devices,
    std::string* error) {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothLowEnergyWrapperFake::
    EnumerateKnownBluetoothLowEnergyGattServiceDevices(
        ScopedVector<BluetoothLowEnergyDeviceInfo>* devices,
        std::string* error) {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothLowEnergyWrapperFake::EnumerateKnownBluetoothLowEnergyServices(
    const base::FilePath& device_path,
    ScopedVector<BluetoothLowEnergyServiceInfo>* services,
    std::string* error) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace win
}  // namespace device
