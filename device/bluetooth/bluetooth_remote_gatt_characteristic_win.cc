// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_win.h"

#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_win.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"

namespace device {

BluetoothRemoteGattCharacteristicWin::BluetoothRemoteGattCharacteristicWin(
    BluetoothRemoteGattServiceWin* parent_service,
    BTH_LE_GATT_CHARACTERISTIC* characteristic_info,
    scoped_refptr<base::SequencedTaskRunner>& ui_task_runner)
    : parent_service_(parent_service),
      characteristic_info_(characteristic_info),
      ui_task_runner_(ui_task_runner),
      weak_ptr_factory_(this) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(parent_service_);
  DCHECK(characteristic_info_);

  adapter_ = static_cast<BluetoothAdapterWin*>(
      parent_service_->GetDevice()->GetAdapter());
  DCHECK(adapter_);
  task_manager_ = adapter_->GetWinBluetoothTaskManager();
  DCHECK(task_manager_);

  characteristic_uuid_ = task_manager_->BluetoothLowEnergyUuidToBluetoothUuid(
      characteristic_info_->CharacteristicUuid);
  characteristic_identifier_ =
      parent_service_->GetIdentifier() +
      std::to_string(characteristic_info_->AttributeHandle);
  Update();
}

BluetoothRemoteGattCharacteristicWin::~BluetoothRemoteGattCharacteristicWin() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  adapter_->NotifyGattCharacteristicRemoved(this);
}

std::string BluetoothRemoteGattCharacteristicWin::GetIdentifier() const {
  return characteristic_identifier_;
}

BluetoothUUID BluetoothRemoteGattCharacteristicWin::GetUUID() const {
  return characteristic_uuid_;
}

bool BluetoothRemoteGattCharacteristicWin::IsLocal() const {
  return false;
}

std::vector<uint8_t>& BluetoothRemoteGattCharacteristicWin::GetValue() const {
  NOTIMPLEMENTED();
  return const_cast<std::vector<uint8_t>&>(characteristic_value_);
}

BluetoothGattService* BluetoothRemoteGattCharacteristicWin::GetService() const {
  return parent_service_;
}

BluetoothGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicWin::GetProperties() const {
  BluetoothGattCharacteristic::Properties properties = PROPERTY_NONE;

  if (characteristic_info_->IsBroadcastable)
    properties = properties | PROPERTY_BROADCAST;
  if (characteristic_info_->IsReadable)
    properties = properties | PROPERTY_READ;
  if (characteristic_info_->IsWritableWithoutResponse)
    properties = properties | PROPERTY_WRITE_WITHOUT_RESPONSE;
  if (characteristic_info_->IsWritable)
    properties = properties | PROPERTY_WRITE;
  if (characteristic_info_->IsNotifiable)
    properties = properties | PROPERTY_NOTIFY;
  if (characteristic_info_->IsIndicatable)
    properties = properties | PROPERTY_INDICATE;
  if (characteristic_info_->IsSignedWritable)
    properties = properties | PROPERTY_AUTHENTICATED_SIGNED_WRITES;
  if (characteristic_info_->HasExtendedProperties)
    properties = properties | PROPERTY_EXTENDED_PROPERTIES;

  // TODO(crbug.com/589304): Information about PROPERTY_RELIABLE_WRITE and
  // PROPERTY_WRITABLE_AUXILIARIES is not available in characteristic_info_
  // (BTH_LE_GATT_CHARACTERISTIC).

  return properties;
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicWin::GetPermissions() const {
  BluetoothGattCharacteristic::Permissions permissions = PERMISSION_NONE;

  if (characteristic_info_->IsReadable)
    permissions = permissions | PERMISSION_READ;
  if (characteristic_info_->IsWritable)
    permissions = permissions | PERMISSION_WRITE;

  return permissions;
}

bool BluetoothRemoteGattCharacteristicWin::IsNotifying() const {
  NOTIMPLEMENTED();
  return false;
}

std::vector<BluetoothGattDescriptor*>
BluetoothRemoteGattCharacteristicWin::GetDescriptors() const {
  NOTIMPLEMENTED();
  return std::vector<BluetoothGattDescriptor*>();
}

BluetoothGattDescriptor* BluetoothRemoteGattCharacteristicWin::GetDescriptor(
    const std::string& identifier) const {
  NOTIMPLEMENTED();
  return nullptr;
}

bool BluetoothRemoteGattCharacteristicWin::AddDescriptor(
    BluetoothGattDescriptor* descriptor) {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothRemoteGattCharacteristicWin::UpdateValue(
    const std::vector<uint8_t>& value) {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothRemoteGattCharacteristicWin::StartNotifySession(
    const NotifySessionCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run(BluetoothGattService::GATT_ERROR_NOT_SUPPORTED);
}

void BluetoothRemoteGattCharacteristicWin::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run(BluetoothGattService::GATT_ERROR_NOT_SUPPORTED);
}

void BluetoothRemoteGattCharacteristicWin::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& new_value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run(BluetoothGattService::GATT_ERROR_NOT_SUPPORTED);
}

void BluetoothRemoteGattCharacteristicWin::Update() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  NOTIMPLEMENTED();
}

uint16_t BluetoothRemoteGattCharacteristicWin::GetAttributeHandle() const {
  return characteristic_info_->AttributeHandle;
}

}  // namespace device.
