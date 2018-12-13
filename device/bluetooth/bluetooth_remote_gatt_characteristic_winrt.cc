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
#include "base/win/post_async_results.h"
#include "base/win/winrt_storage_util.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_discoverer_winrt.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_winrt.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_winrt.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/event_utils_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::BluetoothCacheMode_Uncached;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristicProperties;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattClientCharacteristicConfigurationDescriptorValue;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattClientCharacteristicConfigurationDescriptorValue_Indicate;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattClientCharacteristicConfigurationDescriptorValue_None;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattClientCharacteristicConfigurationDescriptorValue_Notify;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_Success;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattReadResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteOption_WriteWithoutResponse;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteOption_WriteWithResponse;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattCharacteristic;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattCharacteristic3;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattReadResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattReadResult2;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattValueChangedEventArgs;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattWriteResult;
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

  if (value_changed_token_)
    RemoveValueChangedHandler();
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
        base::BindOnce(error_callback,
                       BluetoothRemoteGattService::GATT_ERROR_NOT_PERMITTED));
    return;
  }

  if (pending_read_callbacks_ || pending_write_callbacks_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback,
                       BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }

  ComPtr<IAsyncOperation<GattReadResult*>> read_value_op;
  HRESULT hr = characteristic_->ReadValueWithCacheModeAsync(
      BluetoothCacheMode_Uncached, &read_value_op);
  if (FAILED(hr)) {
    VLOG(2) << "GattCharacteristic::ReadValueWithCacheModeAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback,
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(read_value_op),
      base::BindOnce(&BluetoothRemoteGattCharacteristicWinrt::OnReadValue,
                     weak_ptr_factory_.GetWeakPtr()));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback,
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
        base::BindOnce(error_callback,
                       BluetoothRemoteGattService::GATT_ERROR_NOT_PERMITTED));
    return;
  }

  if (pending_read_callbacks_ || pending_write_callbacks_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback,
                       BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }

  ComPtr<IGattCharacteristic3> characteristic_3;
  HRESULT hr = characteristic_.As(&characteristic_3);
  if (FAILED(hr)) {
    VLOG(2) << "As IGattCharacteristic3 failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback,
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  ComPtr<IBuffer> buffer;
  hr = base::win::CreateIBufferFromData(value.data(), value.size(), &buffer);
  if (FAILED(hr)) {
    VLOG(2) << "base::win::CreateIBufferFromData failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback,
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  ComPtr<IAsyncOperation<GattWriteResult*>> write_value_op;
  hr = characteristic_3->WriteValueWithResultAndOptionAsync(
      buffer.Get(),
      (GetProperties() & PROPERTY_WRITE) ? GattWriteOption_WriteWithResponse
                                         : GattWriteOption_WriteWithoutResponse,

      &write_value_op);
  if (FAILED(hr)) {
    VLOG(2) << "GattCharacteristic::WriteValueWithResultAndOptionAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback,
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(write_value_op),
      base::BindOnce(&BluetoothRemoteGattCharacteristicWinrt::
                         OnWriteValueWithResultAndOption,
                     weak_ptr_factory_.GetWeakPtr()));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback,
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  pending_write_callbacks_ =
      std::make_unique<PendingWriteCallbacks>(callback, error_callback);
}

void BluetoothRemoteGattCharacteristicWinrt::UpdateDescriptors(
    BluetoothGattDiscovererWinrt* gatt_discoverer) {
  const auto* gatt_descriptors =
      gatt_discoverer->GetDescriptors(attribute_handle_);
  DCHECK(gatt_descriptors);

  // Instead of clearing out |descriptors_| and creating each descriptor
  // from scratch, we create a new map and move already existing descriptors
  // into it in order to preserve pointer stability.
  DescriptorMap descriptors;
  for (const auto& gatt_descriptor : *gatt_descriptors) {
    auto descriptor =
        BluetoothRemoteGattDescriptorWinrt::Create(this, gatt_descriptor.Get());
    if (!descriptor)
      continue;

    std::string identifier = descriptor->GetIdentifier();
    auto iter = descriptors_.find(identifier);
    if (iter != descriptors_.end())
      descriptors.emplace(std::move(*iter));
    else
      descriptors.emplace(std::move(identifier), std::move(descriptor));
  }

  std::swap(descriptors, descriptors_);
}

bool BluetoothRemoteGattCharacteristicWinrt::WriteWithoutResponse(
    base::span<const uint8_t> value) {
  if (!(GetProperties() & PROPERTY_WRITE_WITHOUT_RESPONSE))
    return false;

  if (pending_read_callbacks_ || pending_write_callbacks_)
    return false;

  ComPtr<IGattCharacteristic3> characteristic_3;
  HRESULT hr = characteristic_.As(&characteristic_3);
  if (FAILED(hr)) {
    VLOG(2) << "As IGattCharacteristic3 failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  ComPtr<IBuffer> buffer;
  hr = base::win::CreateIBufferFromData(value.data(), value.size(), &buffer);
  if (FAILED(hr)) {
    VLOG(2) << "base::win::CreateIBufferFromData failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  ComPtr<IAsyncOperation<GattWriteResult*>> write_value_op;
  // Note: As we are ignoring the result WriteValueWithOptionAsync() would work
  // as well, but re-using WriteValueWithResultAndOptionAsync() does simplify
  // the testing code and there is no difference in production.
  hr = characteristic_3->WriteValueWithResultAndOptionAsync(
      buffer.Get(), GattWriteOption_WriteWithoutResponse, &write_value_op);
  if (FAILED(hr)) {
    VLOG(2) << "GattCharacteristic::WriteValueWithResultAndOptionAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  // While we are ignoring the response, we still post the async_op in order to
  // extend its lifetime until the operation has completed.
  hr =
      base::win::PostAsyncResults(std::move(write_value_op), base::DoNothing());
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
  value_changed_token_ = AddTypedEventHandler(
      characteristic_.Get(), &IGattCharacteristic::add_ValueChanged,
      base::BindRepeating(
          &BluetoothRemoteGattCharacteristicWinrt::OnValueChanged,
          weak_ptr_factory_.GetWeakPtr()));

  if (!value_changed_token_) {
    VLOG(2) << "Adding Value Changed Handler failed.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback,
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  WriteCccDescriptor(
      (GetProperties() & PROPERTY_NOTIFY)
          ? GattClientCharacteristicConfigurationDescriptorValue_Notify
          : GattClientCharacteristicConfigurationDescriptorValue_Indicate,
      callback, error_callback);
}

void BluetoothRemoteGattCharacteristicWinrt::UnsubscribeFromNotifications(
    BluetoothRemoteGattDescriptor* ccc_descriptor,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  WriteCccDescriptor(
      GattClientCharacteristicConfigurationDescriptorValue_None,
      // Wrap the success and error callbacks in a lambda, so that we can notify
      // callers whether removing the event handler succeeded after the
      // descriptor has been written to.
      base::BindOnce(
          [](base::WeakPtr<BluetoothRemoteGattCharacteristicWinrt>
                 characteristic,
             base::OnceClosure callback, ErrorCallback error_callback) {
            if (characteristic &&
                !characteristic->RemoveValueChangedHandler()) {
              error_callback.Run(BluetoothGattService::GATT_ERROR_FAILED);
              return;
            }

            std::move(callback).Run();
          },
          weak_ptr_factory_.GetWeakPtr(), callback, error_callback),
      error_callback);
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
      attribute_handle_(attribute_handle),
      identifier_(base::StringPrintf("%s/%s_%04x",
                                     service_->GetIdentifier().c_str(),
                                     uuid_.value().c_str(),
                                     attribute_handle_)),
      weak_ptr_factory_(this) {}

void BluetoothRemoteGattCharacteristicWinrt::WriteCccDescriptor(
    GattClientCharacteristicConfigurationDescriptorValue value,
    base::OnceClosure callback,
    const ErrorCallback& error_callback) {
  DCHECK(!pending_notification_callbacks_);
  ComPtr<IGattCharacteristic3> characteristic_3;
  HRESULT hr = characteristic_.As(&characteristic_3);
  if (FAILED(hr)) {
    VLOG(2) << "As IGattCharacteristic3 failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback,
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  ComPtr<IAsyncOperation<GattWriteResult*>> write_ccc_descriptor_op;
  hr = characteristic_3
           ->WriteClientCharacteristicConfigurationDescriptorWithResultAsync(
               value, &write_ccc_descriptor_op);
  if (FAILED(hr)) {
    VLOG(2) << "GattCharacteristic::"
               "WriteClientCharacteristicConfigurationDescriptorWithResultAsync"
               " failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback,
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(write_ccc_descriptor_op),
      base::BindOnce(
          &BluetoothRemoteGattCharacteristicWinrt::OnWriteCccDescriptor,
          weak_ptr_factory_.GetWeakPtr()));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(error_callback,
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  pending_notification_callbacks_ =
      std::make_unique<PendingNotificationCallbacks>(std::move(callback),
                                                     error_callback);
}

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

  if (status != GattCommunicationStatus_Success) {
    VLOG(2) << "Unexpected GattCommunicationStatus: " << status;
    ComPtr<IGattReadResult2> read_result_2;
    hr = read_result.As(&read_result_2);
    if (FAILED(hr)) {
      VLOG(2) << "As IGattReadResult2 failed: "
              << logging::SystemErrorCodeToString(hr);
      pending_read_callbacks->error_callback.Run(
          BluetoothGattService::GATT_ERROR_FAILED);
      return;
    }

    pending_read_callbacks->error_callback.Run(
        BluetoothRemoteGattServiceWinrt::GetGattErrorCode(read_result_2.Get()));
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

void BluetoothRemoteGattCharacteristicWinrt::OnWriteValueWithResultAndOption(
    ComPtr<IGattWriteResult> write_result) {
  DCHECK(pending_write_callbacks_);
  OnWriteImpl(std::move(write_result), std::move(pending_write_callbacks_));
}

void BluetoothRemoteGattCharacteristicWinrt::OnWriteCccDescriptor(
    ComPtr<IGattWriteResult> write_result) {
  OnWriteImpl(std::move(write_result),
              std::move(pending_notification_callbacks_));
}

void BluetoothRemoteGattCharacteristicWinrt::OnWriteImpl(
    ComPtr<IGattWriteResult> write_result,
    std::unique_ptr<PendingWriteCallbacks> callbacks) {
  if (!write_result) {
    callbacks->error_callback.Run(BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }

  GattCommunicationStatus status;
  HRESULT hr = write_result->get_Status(&status);
  if (FAILED(hr)) {
    VLOG(2) << "Getting GATT Communication Status failed: "
            << logging::SystemErrorCodeToString(hr);
    callbacks->error_callback.Run(BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }

  if (status != GattCommunicationStatus_Success) {
    VLOG(2) << "Unexpected GattCommunicationStatus: " << status;
    callbacks->error_callback.Run(
        BluetoothRemoteGattServiceWinrt::GetGattErrorCode(write_result.Get()));
    return;
  }

  std::move(callbacks->callback).Run();
}

void BluetoothRemoteGattCharacteristicWinrt::OnValueChanged(
    IGattCharacteristic* characteristic,
    IGattValueChangedEventArgs* event_args) {
  ComPtr<IBuffer> value;
  HRESULT hr = event_args->get_CharacteristicValue(&value);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Characteristic Value failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  uint8_t* data = nullptr;
  uint32_t length = 0;
  hr = base::win::GetPointerToBufferData(value.Get(), &data, &length);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Pointer To Buffer Data failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  value_.assign(data, data + length);
  service_->GetDevice()->GetAdapter()->NotifyGattCharacteristicValueChanged(
      this, value_);
}

bool BluetoothRemoteGattCharacteristicWinrt::RemoveValueChangedHandler() {
  DCHECK(value_changed_token_);
  HRESULT hr = characteristic_->remove_ValueChanged(*value_changed_token_);
  if (FAILED(hr)) {
    VLOG(2) << "Removing the Value Changed Handler failed: "
            << logging::SystemErrorCodeToString(hr);
  }

  value_changed_token_.reset();
  return SUCCEEDED(hr);
}

}  // namespace device
