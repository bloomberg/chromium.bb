// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_FAKE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_FAKE_H_

#include "device/bluetooth/bluetooth_low_energy_win.h"

namespace device {
namespace win {

// Fake implementation of BluetoothLowEnergyWrapper. Used for BluetoothTestWin.
class BluetoothLowEnergyWrapperFake : public BluetoothLowEnergyWrapper {
 public:
  BluetoothLowEnergyWrapperFake();
  ~BluetoothLowEnergyWrapperFake() override;

  bool EnumerateKnownBluetoothLowEnergyDevices(
      ScopedVector<BluetoothLowEnergyDeviceInfo>* devices,
      std::string* error) override;
  bool EnumerateKnownBluetoothLowEnergyGattServiceDevices(
      ScopedVector<BluetoothLowEnergyDeviceInfo>* devices,
      std::string* error) override;
  bool EnumerateKnownBluetoothLowEnergyServices(
      const base::FilePath& device_path,
      ScopedVector<BluetoothLowEnergyServiceInfo>* services,
      std::string* error) override;
};

}  // namespace win
}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_FAKE_H_
