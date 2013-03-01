// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"

namespace device {

BluetoothAdapterMac::BluetoothAdapterMac()
    : BluetoothAdapter(),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

BluetoothAdapterMac::~BluetoothAdapterMac() {
}

void BluetoothAdapterMac::AddObserver(BluetoothAdapter::Observer* observer) {
}

void BluetoothAdapterMac::RemoveObserver(BluetoothAdapter::Observer* observer) {
}

bool BluetoothAdapterMac::IsInitialized() const {
  return true;
}

bool BluetoothAdapterMac::IsPresent() const {
  return false;
}

bool BluetoothAdapterMac::IsPowered() const {
  return false;
}

void BluetoothAdapterMac::SetPowered(bool powered,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
}

bool BluetoothAdapterMac::IsDiscovering() const {
  return false;
}

bool BluetoothAdapterMac::IsScanning() const {
  return false;
}

void BluetoothAdapterMac::StartDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
}

void BluetoothAdapterMac::StopDiscovering(const base::Closure& callback,
                                          const ErrorCallback& error_callback) {
}

void BluetoothAdapterMac::ReadLocalOutOfBandPairingData(
    const BluetoothOutOfBandPairingDataCallback& callback,
    const ErrorCallback& error_callback) {
}

void BluetoothAdapterMac::TrackDefaultAdapter() {
}

}  // namespace device
