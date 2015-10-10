// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_service_android.h"

#include "device/bluetooth/bluetooth_adapter_android.h"
#include "device/bluetooth/bluetooth_device_android.h"

namespace device {

// static
BluetoothRemoteGattServiceAndroid* BluetoothRemoteGattServiceAndroid::Create(
    BluetoothAdapterAndroid* adapter,
    BluetoothDeviceAndroid* device,
    jobject bluetooth_remote_gatt_service_wrapper,
    std::string instanceId) {
  BluetoothRemoteGattServiceAndroid* service =
      new BluetoothRemoteGattServiceAndroid(adapter, device, instanceId);

  return service;
}

std::string BluetoothRemoteGattServiceAndroid::GetIdentifier() const {
  return instanceId_;
}

device::BluetoothUUID BluetoothRemoteGattServiceAndroid::GetUUID() const {
  NOTIMPLEMENTED();
  return device::BluetoothUUID();
}

bool BluetoothRemoteGattServiceAndroid::IsLocal() const {
  return false;
}

bool BluetoothRemoteGattServiceAndroid::IsPrimary() const {
  NOTIMPLEMENTED();
  return true;
}

device::BluetoothDevice* BluetoothRemoteGattServiceAndroid::GetDevice() const {
  return device_;
}

std::vector<device::BluetoothGattCharacteristic*>
BluetoothRemoteGattServiceAndroid::GetCharacteristics() const {
  NOTIMPLEMENTED();
  std::vector<device::BluetoothGattCharacteristic*> characteristics;
  return characteristics;
}

std::vector<device::BluetoothGattService*>
BluetoothRemoteGattServiceAndroid::GetIncludedServices() const {
  NOTIMPLEMENTED();
  return std::vector<device::BluetoothGattService*>();
}

device::BluetoothGattCharacteristic*
BluetoothRemoteGattServiceAndroid::GetCharacteristic(
    const std::string& identifier) const {
  NOTIMPLEMENTED();
  return nullptr;
}

bool BluetoothRemoteGattServiceAndroid::AddCharacteristic(
    device::BluetoothGattCharacteristic* characteristic) {
  return false;
}

bool BluetoothRemoteGattServiceAndroid::AddIncludedService(
    device::BluetoothGattService* service) {
  return false;
}

void BluetoothRemoteGattServiceAndroid::Register(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run();
}

void BluetoothRemoteGattServiceAndroid::Unregister(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  error_callback.Run();
}

BluetoothRemoteGattServiceAndroid::BluetoothRemoteGattServiceAndroid(
    BluetoothAdapterAndroid* adapter,
    BluetoothDeviceAndroid* device,
    std::string instanceId)
    : adapter_(adapter), device_(device), instanceId_(instanceId) {}

BluetoothRemoteGattServiceAndroid::~BluetoothRemoteGattServiceAndroid() {}

}  // namespace device
