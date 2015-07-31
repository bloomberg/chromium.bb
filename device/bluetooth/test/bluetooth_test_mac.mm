// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/test/bluetooth_test_mac.h"
#include "device/bluetooth/test/mock_bluetooth_central_manager_mac.h"

namespace device {

BluetoothTestMac::BluetoothTestMac() {}

BluetoothTestMac::~BluetoothTestMac() {}

void BluetoothTestMac::SetUp() {}

void BluetoothTestMac::InitWithDefaultAdapter() {
  adapter_mac_ = BluetoothAdapterMac::CreateAdapter().get();
  adapter_ = adapter_mac_;
}

void BluetoothTestMac::InitWithoutDefaultAdapter() {
  adapter_mac_ = BluetoothAdapterMac::CreateAdapterForTest(
                     "", "", message_loop_.task_runner())
                     .get();
  adapter_ = adapter_mac_;

  if (BluetoothAdapterMac::IsLowEnergyAvailable()) {
    id low_energy_central_manager = [[MockCentralManager alloc] init];
    [low_energy_central_manager setState:CBCentralManagerStateUnsupported];
    adapter_mac_->SetCentralManagerForTesting(low_energy_central_manager);
  }
}

void BluetoothTestMac::InitWithFakeAdapter() {
  adapter_mac_ =
      BluetoothAdapterMac::CreateAdapterForTest(
          kTestAdapterName, kTestAdapterAddress, message_loop_.task_runner())
          .get();
  adapter_ = adapter_mac_;

  if (BluetoothAdapterMac::IsLowEnergyAvailable()) {
    id low_energy_central_manager = [[MockCentralManager alloc] init];
    [low_energy_central_manager setState:CBCentralManagerStatePoweredOn];
    adapter_mac_->SetCentralManagerForTesting(low_energy_central_manager);
  }
}

}  // namespace device
