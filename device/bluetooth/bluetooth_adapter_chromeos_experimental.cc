// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_chromeos_experimental.h"

#include <string>

using device::BluetoothAdapter;

namespace chromeos {

BluetoothAdapterChromeOSExperimental::BluetoothAdapterChromeOSExperimental()
    : BluetoothAdapter(),
      weak_ptr_factory_(this) {
}

BluetoothAdapterChromeOSExperimental::~BluetoothAdapterChromeOSExperimental() {
}

void BluetoothAdapterChromeOSExperimental::AddObserver(
    BluetoothAdapter::Observer* observer) {
}

void BluetoothAdapterChromeOSExperimental::RemoveObserver(
    BluetoothAdapter::Observer* observer) {
}

std::string BluetoothAdapterChromeOSExperimental::address() const {
  return std::string();
}

std::string BluetoothAdapterChromeOSExperimental::name() const {
  return std::string();
}

bool BluetoothAdapterChromeOSExperimental::IsInitialized() const {
  return true;
}

bool BluetoothAdapterChromeOSExperimental::IsPresent() const {
  return false;
}

bool BluetoothAdapterChromeOSExperimental::IsPowered() const {
  return false;
}

void BluetoothAdapterChromeOSExperimental::SetPowered(bool powered,
                                          const base::Closure& callback,
                                          const ErrorCallback& error_callback) {
  error_callback.Run();
}

bool BluetoothAdapterChromeOSExperimental::IsDiscovering() const {
  return false;
}

void BluetoothAdapterChromeOSExperimental::StartDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run();
}

void BluetoothAdapterChromeOSExperimental::StopDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run();
}

void BluetoothAdapterChromeOSExperimental::ReadLocalOutOfBandPairingData(
    const BluetoothAdapter::BluetoothOutOfBandPairingDataCallback& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run();
}

}  // namespace chromeos
