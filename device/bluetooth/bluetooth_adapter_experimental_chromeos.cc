// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_experimental_chromeos.h"

#include <string>

using device::BluetoothAdapter;

namespace chromeos {

BluetoothAdapterExperimentalChromeOS::BluetoothAdapterExperimentalChromeOS()
    : BluetoothAdapter(),
      weak_ptr_factory_(this) {
}

BluetoothAdapterExperimentalChromeOS::~BluetoothAdapterExperimentalChromeOS() {
}

void BluetoothAdapterExperimentalChromeOS::AddObserver(
    BluetoothAdapter::Observer* observer) {
}

void BluetoothAdapterExperimentalChromeOS::RemoveObserver(
    BluetoothAdapter::Observer* observer) {
}

std::string BluetoothAdapterExperimentalChromeOS::address() const {
  return std::string();
}

std::string BluetoothAdapterExperimentalChromeOS::name() const {
  return std::string();
}

bool BluetoothAdapterExperimentalChromeOS::IsInitialized() const {
  return true;
}

bool BluetoothAdapterExperimentalChromeOS::IsPresent() const {
  return false;
}

bool BluetoothAdapterExperimentalChromeOS::IsPowered() const {
  return false;
}

void BluetoothAdapterExperimentalChromeOS::SetPowered(bool powered,
                                          const base::Closure& callback,
                                          const ErrorCallback& error_callback) {
  error_callback.Run();
}

bool BluetoothAdapterExperimentalChromeOS::IsDiscovering() const {
  return false;
}

void BluetoothAdapterExperimentalChromeOS::StartDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run();
}

void BluetoothAdapterExperimentalChromeOS::StopDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run();
}

void BluetoothAdapterExperimentalChromeOS::ReadLocalOutOfBandPairingData(
    const BluetoothAdapter::BluetoothOutOfBandPairingDataCallback& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run();
}

}  // namespace chromeos
