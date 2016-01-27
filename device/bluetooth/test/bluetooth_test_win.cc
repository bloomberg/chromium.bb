// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_win.h"

#include "base/bind.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/bluetooth_low_energy_win.h"

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
  adapter_ = new BluetoothAdapterWin(base::Bind(
      &BluetoothTestWin::AdapterInitCallback, base::Unretained(this)));
  adapter_win_ = static_cast<BluetoothAdapterWin*>(adapter_.get());
  adapter_win_->InitForTest(ui_task_runner_, bluetooth_task_runner_);
  BluetoothTaskManagerWin::AdapterState* adapter_state =
      new BluetoothTaskManagerWin::AdapterState();
  adapter_state->name = kTestAdapterName;
  adapter_state->address = kTestAdapterAddress;
  adapter_state->powered = true;
  adapter_win_->AdapterStateChanged(*adapter_state);
}

bool BluetoothTestWin::DenyPermission() {
  return false;
}
}
