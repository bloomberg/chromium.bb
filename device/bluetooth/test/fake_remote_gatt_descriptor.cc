// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_remote_gatt_descriptor.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "device/bluetooth/public/mojom/test/fake_bluetooth.mojom.h"

namespace bluetooth {

FakeRemoteGattDescriptor::FakeRemoteGattDescriptor(
    const std::string& descriptor_id,
    const device::BluetoothUUID& descriptor_uuid,
    device::BluetoothRemoteGattCharacteristic* characteristic)
    : descriptor_id_(descriptor_id),
      descriptor_uuid_(descriptor_uuid),
      characteristic_(characteristic),
      weak_ptr_factory_(this) {}

FakeRemoteGattDescriptor::~FakeRemoteGattDescriptor() = default;

void FakeRemoteGattDescriptor::SetNextReadResponse(
    uint16_t gatt_code,
    const base::Optional<std::vector<uint8_t>>& value) {
  DCHECK(!next_read_response_);
  next_read_response_.emplace(gatt_code, value);
}

bool FakeRemoteGattDescriptor::AllResponsesConsumed() {
  // TODO(crbug.com/569709): Update this when SetNextWriteResponse is
  // implemented.
  return !next_read_response_;
}

std::string FakeRemoteGattDescriptor::GetIdentifier() const {
  return descriptor_id_;
}

device::BluetoothUUID FakeRemoteGattDescriptor::GetUUID() const {
  return descriptor_uuid_;
}

device::BluetoothRemoteGattCharacteristic::Permissions
FakeRemoteGattDescriptor::GetPermissions() const {
  NOTREACHED();
  return device::BluetoothRemoteGattCharacteristic::PERMISSION_NONE;
}

const std::vector<uint8_t>& FakeRemoteGattDescriptor::GetValue() const {
  NOTREACHED();
  return value_;
}

device::BluetoothRemoteGattCharacteristic*
FakeRemoteGattDescriptor::GetCharacteristic() const {
  return characteristic_;
}

void FakeRemoteGattDescriptor::ReadRemoteDescriptor(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&FakeRemoteGattDescriptor::DispatchReadResponse,
                 weak_ptr_factory_.GetWeakPtr(), callback, error_callback));
}

void FakeRemoteGattDescriptor::WriteRemoteDescriptor(
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {}

void FakeRemoteGattDescriptor::DispatchReadResponse(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  DCHECK(next_read_response_);
  uint16_t gatt_code = next_read_response_->gatt_code();
  base::Optional<std::vector<uint8_t>> value = next_read_response_->value();
  next_read_response_.reset();

  if (gatt_code == mojom::kGATTSuccess) {
    DCHECK(value);
    value_ = std::move(value.value());
    callback.Run(value_);
    return;
  } else if (gatt_code == mojom::kGATTInvalidHandle) {
    DCHECK(!value);
    error_callback.Run(device::BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }
}

}  // namespace bluetooth
