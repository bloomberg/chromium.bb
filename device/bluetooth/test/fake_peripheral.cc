// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_peripheral.h"

#include "base/memory/weak_ptr.h"

namespace bluetooth {

FakePeripheral::FakePeripheral(FakeCentral* fake_central,
                               const std::string& address)
    : device::BluetoothDevice(fake_central),
      address_(address),
      system_connected_(false),
      gatt_connected_(false),
      weak_ptr_factory_(this) {}

FakePeripheral::~FakePeripheral() {}

void FakePeripheral::SetName(base::Optional<std::string> name) {
  name_ = std::move(name);
}

void FakePeripheral::SetSystemConnected(bool connected) {
  system_connected_ = connected;
}

void FakePeripheral::SetServiceUUIDs(UUIDSet service_uuids) {
  service_uuids_ = std::move(service_uuids);
}

void FakePeripheral::SetNextGATTConnectionResponse(uint16_t code) {
  DCHECK(!next_connection_response_);
  DCHECK(create_gatt_connection_error_callbacks_.empty());
  next_connection_response_ = code;
}

uint32_t FakePeripheral::GetBluetoothClass() const {
  NOTREACHED();
  return 0;
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
device::BluetoothTransport FakePeripheral::GetType() const {
  NOTREACHED();
  return device::BLUETOOTH_TRANSPORT_INVALID;
}
#endif

std::string FakePeripheral::GetIdentifier() const {
  NOTREACHED();
  return std::string();
}

std::string FakePeripheral::GetAddress() const {
  return address_;
}

device::BluetoothDevice::VendorIDSource FakePeripheral::GetVendorIDSource()
    const {
  NOTREACHED();
  return VENDOR_ID_UNKNOWN;
}

uint16_t FakePeripheral::GetVendorID() const {
  NOTREACHED();
  return 0;
}

uint16_t FakePeripheral::GetProductID() const {
  NOTREACHED();
  return 0;
}

uint16_t FakePeripheral::GetDeviceID() const {
  NOTREACHED();
  return 0;
}

uint16_t FakePeripheral::GetAppearance() const {
  NOTREACHED();
  return 0;
}

base::Optional<std::string> FakePeripheral::GetName() const {
  return name_;
}

base::string16 FakePeripheral::GetNameForDisplay() const {
  return base::string16();
}

bool FakePeripheral::IsPaired() const {
  NOTREACHED();
  return false;
}

bool FakePeripheral::IsConnected() const {
  NOTREACHED();
  return false;
}

bool FakePeripheral::IsGattConnected() const {
  // TODO(crbug.com/728870): Return gatt_connected_ only once system connected
  // peripherals are supported and Web Bluetooth uses them. See issue for more
  // details.
  return system_connected_ || gatt_connected_;
}

bool FakePeripheral::IsConnectable() const {
  NOTREACHED();
  return false;
}

bool FakePeripheral::IsConnecting() const {
  NOTREACHED();
  return false;
}

device::BluetoothDevice::UUIDSet FakePeripheral::GetUUIDs() const {
  return service_uuids_;
}

bool FakePeripheral::ExpectingPinCode() const {
  NOTREACHED();
  return false;
}

bool FakePeripheral::ExpectingPasskey() const {
  NOTREACHED();
  return false;
}

bool FakePeripheral::ExpectingConfirmation() const {
  NOTREACHED();
  return false;
}

void FakePeripheral::GetConnectionInfo(const ConnectionInfoCallback& callback) {
  NOTREACHED();
}

void FakePeripheral::SetConnectionLatency(ConnectionLatency connection_latency,
                                          const base::Closure& callback,
                                          const ErrorCallback& error_callback) {
  NOTREACHED();
}

void FakePeripheral::Connect(PairingDelegate* pairing_delegate,
                             const base::Closure& callback,
                             const ConnectErrorCallback& error_callback) {
  NOTREACHED();
}

void FakePeripheral::SetPinCode(const std::string& pincode) {
  NOTREACHED();
}

void FakePeripheral::SetPasskey(uint32_t passkey) {
  NOTREACHED();
}

void FakePeripheral::ConfirmPairing() {
  NOTREACHED();
}

void FakePeripheral::RejectPairing() {
  NOTREACHED();
}

void FakePeripheral::CancelPairing() {
  NOTREACHED();
}

void FakePeripheral::Disconnect(const base::Closure& callback,
                                const ErrorCallback& error_callback) {
  NOTREACHED();
}

void FakePeripheral::Forget(const base::Closure& callback,
                            const ErrorCallback& error_callback) {
  NOTREACHED();
}

void FakePeripheral::ConnectToService(
    const device::BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTREACHED();
}

void FakePeripheral::ConnectToServiceInsecurely(
    const device::BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTREACHED();
}

void FakePeripheral::CreateGattConnection(
    const GattConnectionCallback& callback,
    const ConnectErrorCallback& error_callback) {
  create_gatt_connection_success_callbacks_.push_back(callback);
  create_gatt_connection_error_callbacks_.push_back(error_callback);

  // TODO(crbug.com/728870): Stop overriding CreateGattConnection once
  // IsGattConnected() is fixed. See issue for more details.
  if (gatt_connected_)
    return DidConnectGatt();

  CreateGattConnectionImpl();
}

void FakePeripheral::CreateGattConnectionImpl() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FakePeripheral::DispatchConnectionResponse,
                            weak_ptr_factory_.GetWeakPtr()));
}

void FakePeripheral::DispatchConnectionResponse() {
  DCHECK(next_connection_response_);

  uint16_t code = next_connection_response_.value();
  next_connection_response_.reset();

  if (code == mojom::kHCISuccess) {
    gatt_connected_ = true;
    DidConnectGatt();
  } else if (code == mojom::kHCIConnectionTimeout) {
    DidFailToConnectGatt(ERROR_FAILED);
  } else {
    DidFailToConnectGatt(ERROR_UNKNOWN);
  }
}

void FakePeripheral::DisconnectGatt() {
}

}  // namespace bluetooth
