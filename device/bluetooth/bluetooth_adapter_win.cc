// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(youngki): Implement this file.

#include "device/bluetooth/bluetooth_adapter_win.h"

#include <string>
#include "base/logging.h"

namespace device {

BluetoothAdapterWin::BluetoothAdapterWin()
    : BluetoothAdapter(),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

BluetoothAdapterWin::~BluetoothAdapterWin() {
}

void BluetoothAdapterWin::AddObserver(BluetoothAdapter::Observer* observer) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWin::RemoveObserver(BluetoothAdapter::Observer* observer) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterWin::IsPresent() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothAdapterWin::IsPowered() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothAdapterWin::SetPowered(
    bool powered,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterWin::IsDiscovering() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothAdapterWin::SetDiscovering(
    bool discovering,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

BluetoothAdapter::ConstDeviceList BluetoothAdapterWin::GetDevices() const {
  NOTIMPLEMENTED();
  return BluetoothAdapter::ConstDeviceList();
}

BluetoothDevice* BluetoothAdapterWin::GetDevice(const std::string& address) {
  NOTIMPLEMENTED();
  return NULL;
}

const BluetoothDevice* BluetoothAdapterWin::GetDevice(
    const std::string& address) const {
  NOTIMPLEMENTED();
  return NULL;
}

void BluetoothAdapterWin::ReadLocalOutOfBandPairingData(
    const BluetoothOutOfBandPairingDataCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

}  // namespace device
