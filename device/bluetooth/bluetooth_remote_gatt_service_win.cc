// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_service_win.h"

namespace device {
BluetoothRemoteGattServiceWin::BluetoothRemoteGattServiceWin(
    BluetoothDeviceWin* device,
    base::FilePath service_path,
    BluetoothUUID service_uuid,
    uint16_t service_attribute_handle,
    bool is_primary,
    BluetoothRemoteGattServiceWin* parent_service,
    scoped_refptr<base::SequencedTaskRunner>& ui_task_runner)
    : device_(device),
      service_path_(service_path),
      service_uuid_(service_uuid),
      service_attribute_handle_(service_attribute_handle),
      is_primary_(is_primary),
      parent_service_(parent_service),
      ui_task_runner_(ui_task_runner) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!service_path_.empty());
  DCHECK(service_uuid_.IsValid());
  DCHECK(service_attribute_handle_);
  DCHECK(device_);
  if (!is_primary_)
    DCHECK(parent_service_);
  Update();
}

BluetoothRemoteGattServiceWin::~BluetoothRemoteGattServiceWin() {}

std::string BluetoothRemoteGattServiceWin::GetIdentifier() const {
  std::string identifier =
      service_uuid_.value() + "_" + std::to_string(service_attribute_handle_);
  if (is_primary_)
    return device_->GetIdentifier() + "/" + identifier;
  else
    return parent_service_->GetIdentifier() + "/" + identifier;
}

BluetoothUUID BluetoothRemoteGattServiceWin::GetUUID() const {
  return const_cast<BluetoothUUID&>(service_uuid_);
}

bool BluetoothRemoteGattServiceWin::IsLocal() const {
  return false;
}

bool BluetoothRemoteGattServiceWin::IsPrimary() const {
  return is_primary_;
}

BluetoothDevice* BluetoothRemoteGattServiceWin::GetDevice() const {
  return device_;
}

std::vector<BluetoothGattCharacteristic*>
BluetoothRemoteGattServiceWin::GetCharacteristics() const {
  NOTIMPLEMENTED();
  return std::vector<BluetoothGattCharacteristic*>();
}

std::vector<BluetoothGattService*>
BluetoothRemoteGattServiceWin::GetIncludedServices() const {
  NOTIMPLEMENTED();
  return std::vector<BluetoothGattService*>();
}

BluetoothGattCharacteristic* BluetoothRemoteGattServiceWin::GetCharacteristic(
    const std::string& identifier) const {
  NOTIMPLEMENTED();
  return nullptr;
}

bool BluetoothRemoteGattServiceWin::AddCharacteristic(
    device::BluetoothGattCharacteristic* characteristic) {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothRemoteGattServiceWin::AddIncludedService(
    device::BluetoothGattService* service) {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothRemoteGattServiceWin::Register(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run();
}

void BluetoothRemoteGattServiceWin::Unregister(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run();
}

void BluetoothRemoteGattServiceWin::Update() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  NOTIMPLEMENTED();
}

uint16_t BluetoothRemoteGattServiceWin::GetAttributeHandle() {
  return service_attribute_handle_;
}

}  // namespace device.
