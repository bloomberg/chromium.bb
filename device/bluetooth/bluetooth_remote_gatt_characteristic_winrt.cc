// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_characteristic_winrt.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/winrt_storage_util.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/event_utils_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristicProperties;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_Success;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattReadResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteOption_WriteWithResponse;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteOption_WriteWithoutResponse;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattCharacteristic;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattReadResult;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Storage::Streams::IBuffer;
using Microsoft::WRL::ComPtr;

}  // namespace

// static
std::unique_ptr<BluetoothRemoteGattCharacteristicWinrt>
BluetoothRemoteGattCharacteristicWinrt::Create(
    BluetoothRemoteGattService* service,
    ComPtr<IGattCharacteristic> characteristic) {
  DCHECK(characteristic);
  GUID guid;
  HRESULT hr = characteristic->get_Uuid(&guid);
  if (FAILED(hr)) {
    VLOG(2) << "Getting UUID failed: " << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  GattCharacteristicProperties properties;
  hr = characteristic->get_CharacteristicProperties(&properties);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Properties failed: "
            << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  uint16_t attribute_handle;
  hr = characteristic->get_AttributeHandle(&attribute_handle);
  if (FAILED(hr)) {
    VLOG(2) << "Getting AttributeHandle failed: "
            << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return base::WrapUnique(new BluetoothRemoteGattCharacteristicWinrt(
      service, std::move(characteristic), BluetoothUUID(guid), properties,
      attribute_handle));
}

BluetoothRemoteGattCharacteristicWinrt::
    ~BluetoothRemoteGattCharacteristicWinrt() {
  if (pending_read_callbacks_) {
    pending_read_callbacks_->error_callback.Run(
        BluetoothGattService::GATT_ERROR_FAILED);
  }

  if (pending_write_callbacks_) {
    pending_write_callbacks_->error_callback.Run(
        BluetoothGattService::GATT_ERROR_FAILED);
  }
}

std::string BluetoothRemoteGattCharacteristicWinrt::GetIdentifier() const {
  return identifier_;
}

BluetoothUUID BluetoothRemoteGattCharacteristicWinrt::GetUUID() const {
  return uuid_;
}

BluetoothGattCharacteristic::Properties
BluetoothRemoteGattCharacteristicWinrt::GetProperties() const {
  return properties_;
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattCharacteristicWinrt::GetPermissions() const {
  NOTIMPLEMENTED();
  return Permissions();
}

const std::vector<uint8_t>& BluetoothRemoteGattCharacteristicWinrt::GetValue()
    const {
  return value_;
}

BluetoothRemoteGattService* BluetoothRemoteGattCharacteristicWinrt::GetService()
    const {
  return service_;
}

void BluetoothRemoteGattCharacteristicWinrt::ReadRemoteCharacteristic(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  if (!(GetProperties() & PROPERTY_READ)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   BluetoothRemoteGattService::GATT_ERROR_NOT_PERMITTED));
    return;
  }

  if (pending_read_callbacks_ || pending_write_callbacks_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }

  ComPtr<IAsyncOperation<GattReadResult*>> read_value_op;
  HRESULT hr = characteristic_->ReadValueAsync(&read_value_op);
  if (FAILED(hr)) {
    VLOG(2) << "GattCharacteristic::ReadValueAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(error_callback,
                              BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  hr = PostAsyncResults(
      std::move(read_value_op),
      base::BindOnce(&BluetoothRemoteGattCharacteristicWinrt::OnReadValue,
                     weak_ptr_factory_.GetWeakPtr()));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(error_callback,
                              BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  pending_read_callbacks_ =
      std::make_unique<PendingReadCallbacks>(callback, error_callback);
}

void BluetoothRemoteGattCharacteristicWinrt::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!(GetProperties() & PROPERTY_WRITE) &&
      !(GetProperties() & PROPERTY_WRITE_WITHOUT_RESPONSE)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   BluetoothRemoteGattService::GATT_ERROR_NOT_PERMITTED));
    return;
  }

  if (pending_read_callbacks_ || pending_write_callbacks_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(error_callback,
                   BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }

  ComPtr<IBuffer> buffer;
  HRESULT hr =
      base::win::CreateIBufferFromData(value.data(), value.size(), &buffer);
  if (FAILED(hr)) {
    VLOG(2) << "base::win::CreateIBufferFromData failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(error_callback,
                              BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  ComPtr<IAsyncOperation<GattCommunicationStatus>> write_value_op;
  hr = characteristic_->WriteValueWithOptionAsync(
      buffer.Get(),
      (GetProperties() & PROPERTY_WRITE) ? GattWriteOption_WriteWithResponse
                                         : GattWriteOption_WriteWithoutResponse,

      &write_value_op);
  if (FAILED(hr)) {
    VLOG(2) << "GattCharacteristic::WriteValueWithOptionAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(error_callback,
                              BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  hr = PostAsyncResults(
      std::move(write_value_op),
      base::BindOnce(
          &BluetoothRemoteGattCharacteristicWinrt::OnWriteValueWithOption,
          weak_ptr_factory_.GetWeakPtr()));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(error_callback,
                              BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  pending_write_callbacks_ =
      std::make_unique<PendingWriteCallbacks>(callback, error_callback);
}

bool BluetoothRemoteGattCharacteristicWinrt::WriteWithoutResponse(
    base::span<const uint8_t> value) {
  if (!(GetProperties() & PROPERTY_WRITE_WITHOUT_RESPONSE))
    return false;

  if (pending_read_callbacks_ || pending_write_callbacks_)
    return false;

  ComPtr<IBuffer> buffer;
  HRESULT hr =
      base::win::CreateIBufferFromData(value.data(), value.size(), &buffer);
  if (FAILED(hr)) {
    VLOG(2) << "base::win::CreateIBufferFromData failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  ComPtr<IAsyncOperation<GattCommunicationStatus>> write_value_op;
  hr = characteristic_->WriteValueWithOptionAsync(
      buffer.Get(), GattWriteOption_WriteWithoutResponse, &write_value_op);
  if (FAILED(hr)) {
    VLOG(2) << "GattCharacteristic::WriteValueWithOptionAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  // While we are ignoring the response, we still post the async_op in order to
  // extend its lifetime until the operation has completed.
  hr = PostAsyncResults(std::move(write_value_op), base::DoNothing());
  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  return true;
}

IGattCharacteristic*
BluetoothRemoteGattCharacteristicWinrt::GetCharacteristicForTesting() {
  return characteristic_.Get();
}

void BluetoothRemoteGattCharacteristicWinrt::SubscribeToNotifications(
    BluetoothRemoteGattDescriptor* ccc_descriptor,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothRemoteGattCharacteristicWinrt::UnsubscribeFromNotifications(
    BluetoothRemoteGattDescriptor* ccc_descriptor,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

BluetoothRemoteGattCharacteristicWinrt::PendingReadCallbacks::
    PendingReadCallbacks(ValueCallback callback, ErrorCallback error_callback)
    : callback(std::move(callback)),
      error_callback(std::move(error_callback)) {}
BluetoothRemoteGattCharacteristicWinrt::PendingReadCallbacks::
    ~PendingReadCallbacks() = default;

BluetoothRemoteGattCharacteristicWinrt::PendingWriteCallbacks::
    PendingWriteCallbacks(base::OnceClosure callback,
                          ErrorCallback error_callback)
    : callback(std::move(callback)),
      error_callback(std::move(error_callback)) {}
BluetoothRemoteGattCharacteristicWinrt::PendingWriteCallbacks::
    ~PendingWriteCallbacks() = default;

BluetoothRemoteGattCharacteristicWinrt::BluetoothRemoteGattCharacteristicWinrt(
    BluetoothRemoteGattService* service,
    ComPtr<IGattCharacteristic> characteristic,
    BluetoothUUID uuid,
    Properties properties,
    uint16_t attribute_handle)
    : service_(service),
      characteristic_(std::move(characteristic)),
      uuid_(std::move(uuid)),
      properties_(properties),
      identifier_(base::StringPrintf("%s/%s_%04x",
                                     service_->GetIdentifier().c_str(),
                                     uuid_.value().c_str(),
                                     attribute_handle)),
      weak_ptr_factory_(this) {}

void BluetoothRemoteGattCharacteristicWinrt::OnReadValue(
    ComPtr<IGattReadResult> read_result) {
  DCHECK(pending_read_callbacks_);
  auto pending_read_callbacks = std::move(pending_read_callbacks_);

  if (!read_result) {
    pending_read_callbacks->error_callback.Run(
        BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }

  GattCommunicationStatus status;
  HRESULT hr = read_result->get_Status(&status);
  if (FAILED(hr)) {
    VLOG(2) << "Getting GATT Communication Status failed: "
            << logging::SystemErrorCodeToString(hr);
    pending_read_callbacks->error_callback.Run(
        BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }

  // TODO(https://crbug.com/821766): Implement more fine-grained error reporting
  // by inspecting protocol errors.
  if (status != GattCommunicationStatus_Success) {
    VLOG(2) << "Unexpected GattCommunicationStatus: " << status;
    pending_read_callbacks->error_callback.Run(
        BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }

  ComPtr<IBuffer> value;
  hr = read_result->get_Value(&value);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Characteristic Value failed: "
            << logging::SystemErrorCodeToString(hr);
    pending_read_callbacks->error_callback.Run(
        BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }

  uint8_t* data = nullptr;
  uint32_t length = 0;
  hr = base::win::GetPointerToBufferData(value.Get(), &data, &length);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Pointer To Buffer Data failed: "
            << logging::SystemErrorCodeToString(hr);
    pending_read_callbacks->error_callback.Run(
        BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }

  value_.assign(data, data + length);
  pending_read_callbacks->callback.Run(value_);
}

void BluetoothRemoteGattCharacteristicWinrt::OnWriteValueWithOption(
    GattCommunicationStatus status) {
  if (!pending_write_callbacks_)
    return;

  auto pending_write_callbacks = std::move(pending_write_callbacks_);
  // TODO(https://crbug.com/821766): Implement more fine-grained error reporting
  // by inspecting protocol errors.
  if (status != GattCommunicationStatus_Success) {
    VLOG(2) << "Unexpected GattCommunicationStatus: " << status;
    pending_write_callbacks->error_callback.Run(
        BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }

  std::move(pending_write_callbacks->callback).Run();
}

}  // namespace device
