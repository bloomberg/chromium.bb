// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_win.h"

#include "base/bind.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_win.h"
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
      characteristic_added_notified_(false),
      weak_ptr_factory_(this) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(parent_service_);
  DCHECK(characteristic_info_);

  task_manager_ =
      parent_service_->GetWinAdapter()->GetWinBluetoothTaskManager();
  DCHECK(task_manager_);
  characteristic_uuid_ =
      BluetoothTaskManagerWin::BluetoothLowEnergyUuidToBluetoothUuid(
          characteristic_info_->CharacteristicUuid);
  characteristic_identifier_ =
      parent_service_->GetIdentifier() + "_" +
      std::to_string(characteristic_info_->AttributeHandle);
  Update();
}

BluetoothRemoteGattCharacteristicWin::~BluetoothRemoteGattCharacteristicWin() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  parent_service_->GetWinAdapter()->NotifyGattCharacteristicRemoved(this);
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
  std::vector<BluetoothGattDescriptor*> descriptors;
  for (const auto& descriptor : included_descriptors_)
    descriptors.push_back(descriptor.second.get());
  return descriptors;
}

BluetoothGattDescriptor* BluetoothRemoteGattCharacteristicWin::GetDescriptor(
    const std::string& identifier) const {
  GattDescriptorMap::const_iterator it = included_descriptors_.find(identifier);
  if (it != included_descriptors_.end())
    return it->second.get();
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

  task_manager_->PostGetGattIncludedDescriptors(
      parent_service_->GetServicePath(), characteristic_info_.get(),
      base::Bind(&BluetoothRemoteGattCharacteristicWin::
                     OnGetIncludedDescriptorsCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

uint16_t BluetoothRemoteGattCharacteristicWin::GetAttributeHandle() const {
  return characteristic_info_->AttributeHandle;
}

void BluetoothRemoteGattCharacteristicWin::OnGetIncludedDescriptorsCallback(
    scoped_ptr<BTH_LE_GATT_DESCRIPTOR> descriptors,
    uint16_t num,
    HRESULT hr) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  UpdateIncludedDescriptors(descriptors.get(), num);
  if (!characteristic_added_notified_) {
    characteristic_added_notified_ = true;
    parent_service_->GetWinAdapter()->NotifyGattCharacteristicAdded(this);
  }
}

void BluetoothRemoteGattCharacteristicWin::UpdateIncludedDescriptors(
    PBTH_LE_GATT_DESCRIPTOR descriptors,
    uint16_t num) {
  if (num == 0) {
    included_descriptors_.clear();
    return;
  }

  // First, remove descriptors that no longer exist.
  std::vector<std::string> to_be_removed;
  for (const auto& d : included_descriptors_) {
    if (!DoesDescriptorExist(descriptors, num, d.second.get()))
      to_be_removed.push_back(d.second->GetIdentifier());
  }
  for (auto id : to_be_removed)
    included_descriptors_.erase(id);

  // Return if no new descriptors have been added.
  if (included_descriptors_.size() == num)
    return;

  // Add new descriptors.
  for (uint16_t i = 0; i < num; i++) {
    if (!IsDescriptorDiscovered(descriptors[i].DescriptorUuid,
                                descriptors[i].AttributeHandle)) {
      PBTH_LE_GATT_DESCRIPTOR win_descriptor_info =
          new BTH_LE_GATT_DESCRIPTOR();
      *win_descriptor_info = descriptors[i];
      BluetoothRemoteGattDescriptorWin* descriptor =
          new BluetoothRemoteGattDescriptorWin(this, win_descriptor_info,
                                               ui_task_runner_);
      included_descriptors_[descriptor->GetIdentifier()] =
          make_scoped_ptr(descriptor);
    }
  }
}

bool BluetoothRemoteGattCharacteristicWin::IsDescriptorDiscovered(
    BTH_LE_UUID& uuid,
    uint16_t attribute_handle) {
  BluetoothUUID bt_uuid =
      BluetoothTaskManagerWin::BluetoothLowEnergyUuidToBluetoothUuid(uuid);
  for (const auto& d : included_descriptors_) {
    if (bt_uuid == d.second->GetUUID() &&
        attribute_handle == d.second->GetAttributeHandle()) {
      return true;
    }
  }
  return false;
}

bool BluetoothRemoteGattCharacteristicWin::DoesDescriptorExist(
    PBTH_LE_GATT_DESCRIPTOR descriptors,
    uint16_t num,
    BluetoothRemoteGattDescriptorWin* descriptor) {
  for (uint16_t i = 0; i < num; i++) {
    BluetoothUUID uuid =
        BluetoothTaskManagerWin::BluetoothLowEnergyUuidToBluetoothUuid(
            descriptors[i].DescriptorUuid);
    if (descriptor->GetUUID() == uuid &&
        descriptor->GetAttributeHandle() == descriptors[i].AttributeHandle) {
      return true;
    }
  }
  return false;
}

}  // namespace device.
