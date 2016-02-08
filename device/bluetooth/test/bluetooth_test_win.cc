// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_win.h"

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/bluetooth_low_energy_win.h"

namespace {

BLUETOOTH_ADDRESS CanonicalStringToBLUETOOTH_ADDRESS(
    std::string device_address) {
  BLUETOOTH_ADDRESS win_addr;
  unsigned int data[6];
  int result =
      sscanf_s(device_address.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X",
               &data[5], &data[4], &data[3], &data[2], &data[1], &data[0]);
  CHECK(result == 6);
  for (int i = 0; i < 6; i++) {
    win_addr.rgBytes[i] = data[i];
  }
  return win_addr;
}

}  // namespace

namespace device {
BluetoothTestWin::BluetoothTestWin()
    : ui_task_runner_(new base::TestSimpleTaskRunner()),
      bluetooth_task_runner_(new base::TestSimpleTaskRunner()) {}
BluetoothTestWin::~BluetoothTestWin() {}

bool BluetoothTestWin::PlatformSupportsLowEnergy() {
  return win::IsBluetoothLowEnergySupported();
}

void BluetoothTestWin::AdapterInitCallback() {}

void BluetoothTestWin::InitWithDefaultAdapter() {
  adapter_ = new BluetoothAdapterWin(base::Bind(
      &BluetoothTestWin::AdapterInitCallback, base::Unretained(this)));
  adapter_win_ = static_cast<BluetoothAdapterWin*>(adapter_.get());
  adapter_win_->Init();
}

void BluetoothTestWin::InitWithoutDefaultAdapter() {
  adapter_ = new BluetoothAdapterWin(base::Bind(
      &BluetoothTestWin::AdapterInitCallback, base::Unretained(this)));
  adapter_win_ = static_cast<BluetoothAdapterWin*>(adapter_.get());
  adapter_win_->InitForTest(ui_task_runner_, bluetooth_task_runner_);
}

void BluetoothTestWin::InitWithFakeAdapter() {
  fake_bt_classic_wrapper_ = new win::BluetoothClassicWrapperFake();
  fake_bt_le_wrapper_ = new win::BluetoothLowEnergyWrapperFake();
  win::BluetoothClassicWrapper::SetInstanceForTest(fake_bt_classic_wrapper_);
  win::BluetoothLowEnergyWrapper::SetInstanceForTest(fake_bt_le_wrapper_);
  fake_bt_classic_wrapper_->SimulateARadio(
      base::SysUTF8ToWide(kTestAdapterName),
      CanonicalStringToBLUETOOTH_ADDRESS(kTestAdapterAddress));

  adapter_ = new BluetoothAdapterWin(base::Bind(
      &BluetoothTestWin::AdapterInitCallback, base::Unretained(this)));
  adapter_win_ = static_cast<BluetoothAdapterWin*>(adapter_.get());
  adapter_win_->InitForTest(ui_task_runner_, bluetooth_task_runner_);
  bluetooth_task_runner_->RunPendingTasks();
  ui_task_runner_->RunPendingTasks();
}

bool BluetoothTestWin::DenyPermission() {
  return false;
}
}
