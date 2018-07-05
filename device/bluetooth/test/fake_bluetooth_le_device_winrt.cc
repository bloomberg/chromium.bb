// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_le_device_winrt.h"

#include <wrl/client.h>

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/async_operation.h"
#include "device/bluetooth/test/bluetooth_test_win.h"
#include "device/bluetooth/test/fake_gatt_device_services_result_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::BluetoothCacheMode;
using ABI::Windows::Devices::Bluetooth::BluetoothConnectionStatus;
using ABI::Windows::Devices::Bluetooth::BluetoothConnectionStatus_Connected;
using ABI::Windows::Devices::Bluetooth::BluetoothConnectionStatus_Disconnected;
using ABI::Windows::Devices::Bluetooth::BluetoothLEDevice;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_AccessDenied;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_ProtocolError;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_Success;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_Unreachable;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceService;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceServicesResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattDeviceService;
using ABI::Windows::Devices::Enumeration::DeviceAccessStatus;
using ABI::Windows::Devices::Enumeration::IDeviceAccessInformation;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::ITypedEventHandler;
using Microsoft::WRL::Make;

}  // namespace

FakeBluetoothLEDeviceWinrt::FakeBluetoothLEDeviceWinrt(
    BluetoothTestWinrt* bluetooth_test_winrt)
    : bluetooth_test_winrt_(bluetooth_test_winrt) {}

FakeBluetoothLEDeviceWinrt::~FakeBluetoothLEDeviceWinrt() = default;

HRESULT FakeBluetoothLEDeviceWinrt::get_DeviceId(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_Name(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_GattServices(
    IVectorView<GattDeviceService*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_ConnectionStatus(
    BluetoothConnectionStatus* value) {
  *value = status_;
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_BluetoothAddress(uint64_t* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattService(
    GUID service_uuid,
    IGattDeviceService** service) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::add_NameChanged(
    ITypedEventHandler<BluetoothLEDevice*, IInspectable*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::remove_NameChanged(
    EventRegistrationToken token) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::add_GattServicesChanged(
    ITypedEventHandler<BluetoothLEDevice*, IInspectable*>* handler,
    EventRegistrationToken* token) {
  gatt_services_changed_handler_ = handler;
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::remove_GattServicesChanged(
    EventRegistrationToken token) {
  gatt_services_changed_handler_ = nullptr;
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::add_ConnectionStatusChanged(
    ITypedEventHandler<BluetoothLEDevice*, IInspectable*>* handler,
    EventRegistrationToken* token) {
  connection_status_changed_handler_ = handler;
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::remove_ConnectionStatusChanged(
    EventRegistrationToken token) {
  connection_status_changed_handler_ = nullptr;
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_DeviceAccessInformation(
    IDeviceAccessInformation** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::RequestAccessAsync(
    IAsyncOperation<DeviceAccessStatus>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattServicesAsync(
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  auto async_op = Make<base::win::AsyncOperation<GattDeviceServicesResult*>>();
  gatt_services_callback_ = async_op->callback();
  *operation = async_op.Detach();
  bluetooth_test_winrt_->OnFakeBluetoothDeviceConnectGattCalled();
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattServicesWithCacheModeAsync(
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattServicesForUuidAsync(
    GUID service_uuid,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattServicesForUuidWithCacheModeAsync(
    GUID service_uuid,
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::Close() {
  bluetooth_test_winrt_->OnFakeBluetoothGattDisconnect();
  return S_OK;
}

void FakeBluetoothLEDeviceWinrt::SimulateGattConnection() {
  status_ = BluetoothConnectionStatus_Connected;
  connection_status_changed_handler_->Invoke(this, nullptr);
}

void FakeBluetoothLEDeviceWinrt ::SimulateGattConnectionError(
    BluetoothDevice::ConnectErrorCode error_code) {
  if (!gatt_services_callback_)
    return;

  std::move(gatt_services_callback_)
      .Run(Make<FakeGattDeviceServicesResultWinrt>(
          GattCommunicationStatus_ProtocolError));
}

void FakeBluetoothLEDeviceWinrt::SimulateGattDisconnection() {
  if (status_ == BluetoothConnectionStatus_Disconnected) {
    DCHECK(gatt_services_callback_);
    std::move(gatt_services_callback_)
        .Run(Make<FakeGattDeviceServicesResultWinrt>(
            GattCommunicationStatus_Unreachable));
    return;
  }

  status_ = BluetoothConnectionStatus_Disconnected;
  connection_status_changed_handler_->Invoke(this, nullptr);
}

void FakeBluetoothLEDeviceWinrt::SimulateGattServicesDiscovered(
    const std::vector<std::string>& uuids) {
  DCHECK(gatt_services_callback_);
  std::move(gatt_services_callback_)
      .Run(Make<FakeGattDeviceServicesResultWinrt>(
          GattCommunicationStatus_Success, uuids));
}

void FakeBluetoothLEDeviceWinrt::SimulateGattServicesChanged() {
  DCHECK(gatt_services_changed_handler_);
  gatt_services_changed_handler_->Invoke(this, nullptr);
}

void FakeBluetoothLEDeviceWinrt::SimulateGattServicesDiscoveryError() {
  DCHECK(gatt_services_callback_);
  std::move(gatt_services_callback_)
      .Run(Make<FakeGattDeviceServicesResultWinrt>(
          GattCommunicationStatus_ProtocolError));
}

FakeBluetoothLEDeviceStaticsWinrt::FakeBluetoothLEDeviceStaticsWinrt(
    BluetoothTestWinrt* bluetooth_test_winrt)
    : bluetooth_test_winrt_(bluetooth_test_winrt) {}

FakeBluetoothLEDeviceStaticsWinrt::~FakeBluetoothLEDeviceStaticsWinrt() =
    default;

HRESULT FakeBluetoothLEDeviceStaticsWinrt::FromIdAsync(
    HSTRING device_id,
    IAsyncOperation<BluetoothLEDevice*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceStaticsWinrt::FromBluetoothAddressAsync(
    uint64_t bluetooth_address,
    IAsyncOperation<BluetoothLEDevice*>** operation) {
  auto async_op = Make<base::win::AsyncOperation<BluetoothLEDevice*>>();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(async_op->callback(),
                     Make<FakeBluetoothLEDeviceWinrt>(bluetooth_test_winrt_)));
  *operation = async_op.Detach();
  return S_OK;
}

HRESULT FakeBluetoothLEDeviceStaticsWinrt::GetDeviceSelector(
    HSTRING* device_selector) {
  return E_NOTIMPL;
}

}  // namespace device
