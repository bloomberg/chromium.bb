// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_winrt.h"

#include "base/logging.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"

namespace device {

std::string BluetoothAdapterWinrt::GetAddress() const {
  NOTIMPLEMENTED();
  return std::string();
}

std::string BluetoothAdapterWinrt::GetName() const {
  NOTIMPLEMENTED();
  return std::string();
}

void BluetoothAdapterWinrt::SetName(const std::string& name,
                                    const base::Closure& callback,
                                    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterWinrt::IsInitialized() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothAdapterWinrt::IsPresent() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothAdapterWinrt::IsPowered() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothAdapterWinrt::IsDiscoverable() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothAdapterWinrt::SetDiscoverable(
    bool discoverable,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterWinrt::IsDiscovering() const {
  NOTIMPLEMENTED();
  return false;
}

BluetoothAdapter::UUIDList BluetoothAdapterWinrt::GetUUIDs() const {
  NOTIMPLEMENTED();
  return UUIDList();
}

void BluetoothAdapterWinrt::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWinrt::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWinrt::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    const CreateAdvertisementCallback& callback,
    const AdvertisementErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

BluetoothLocalGattService* BluetoothAdapterWinrt::GetGattService(
    const std::string& identifier) const {
  NOTIMPLEMENTED();
  return nullptr;
}

bool BluetoothAdapterWinrt::SetPoweredImpl(bool powered) {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothAdapterWinrt::AddDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    DiscoverySessionErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWinrt::RemoveDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    DiscoverySessionErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWinrt::SetDiscoveryFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const base::Closure& callback,
    DiscoverySessionErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWinrt::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
  NOTIMPLEMENTED();
}

BluetoothAdapterWinrt::BluetoothAdapterWinrt() = default;
BluetoothAdapterWinrt::~BluetoothAdapterWinrt() = default;

}  // namespace device
