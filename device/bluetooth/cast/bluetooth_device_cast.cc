// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/cast/bluetooth_device_cast.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"

namespace device {
namespace {

// BluetoothUUID expects uuids in this format.
const char kServiceUuid128BitFormat[] =
    "%02hhx%02hhx%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx-"
    "%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx";

// http://www.argenox.com/bluetooth-low-energy-ble-v4-0-development/library/a-ble-advertising-primer.
const uint8_t kComplete128BitServiceUuids = 0x07;

BluetoothDevice::UUIDSet ExtractServiceUuids(
    const chromecast::bluetooth::LeScanManager::ScanResult& result,
    const std::string& debug_name) {
  BluetoothDevice::UUIDSet uuids;

  // Handle 128-bit UUIDs.
  // TODO(slan): Parse more services.
  auto it = result.type_to_data.find(kComplete128BitServiceUuids);
  if (it != result.type_to_data.end()) {
    // Iterate through this data in 16-byte chunks.
    const auto& data = it->second;
    for (size_t i = 0; i < data.size(); i += 16) {
      auto raw = base::StringPrintf(
          kServiceUuid128BitFormat, data[i + 15], data[i + 14], data[i + 13],
          data[i + 12], data[i + 11], data[i + 10], data[i + 9], data[i + 8],
          data[i + 7], data[i + 6], data[i + 5], data[i + 4], data[i + 3],
          data[i + 2], data[i + 1], data[i + 0]);
      BluetoothUUID uuid(raw);
      LOG_IF(ERROR, !uuid.IsValid()) << raw << " is not a valid uuid.";

      uuids.insert(std::move(uuid));
    }
  }
  return uuids;
}

}  // namespace

BluetoothDeviceCast::BluetoothDeviceCast(
    BluetoothAdapter* adapter,
    scoped_refptr<chromecast::bluetooth::RemoteDevice> device)
    : BluetoothDevice(adapter),
      remote_device_(std::move(device)),
      address_(CanonicalizeAddress(
          chromecast::bluetooth::util::AddrToString(remote_device_->addr()))),
      weak_factory_(this) {}

BluetoothDeviceCast::~BluetoothDeviceCast() {}

uint32_t BluetoothDeviceCast::GetBluetoothClass() const {
  return 0;
}

BluetoothTransport BluetoothDeviceCast::GetType() const {
  return BLUETOOTH_TRANSPORT_LE;
}

std::string BluetoothDeviceCast::GetAddress() const {
  return address_;
}

BluetoothDevice::VendorIDSource BluetoothDeviceCast::GetVendorIDSource() const {
  return VENDOR_ID_UNKNOWN;
}

uint16_t BluetoothDeviceCast::GetVendorID() const {
  return 0;
}

uint16_t BluetoothDeviceCast::GetProductID() const {
  return 0;
}

uint16_t BluetoothDeviceCast::GetDeviceID() const {
  return 0;
}

uint16_t BluetoothDeviceCast::GetAppearance() const {
  return 0;
}

base::Optional<std::string> BluetoothDeviceCast::GetName() const {
  return name_;
}

bool BluetoothDeviceCast::IsPaired() const {
  return false;
}

bool BluetoothDeviceCast::IsConnected() const {
  return connected_;
}

bool BluetoothDeviceCast::IsGattConnected() const {
  return IsConnected();
}

bool BluetoothDeviceCast::IsConnectable() const {
  NOTREACHED() << "This is only called on ChromeOS";
  return true;
}

bool BluetoothDeviceCast::IsConnecting() const {
  return pending_connect_;
}

BluetoothDevice::UUIDSet BluetoothDeviceCast::GetUUIDs() const {
  // TODO(slan): Remove if we do not need this.
  return BluetoothDevice::GetUUIDs();
}

base::Optional<int8_t> BluetoothDeviceCast::GetInquiryRSSI() const {
  // TODO(slan): Plumb this from the type_to_data field of ScanResult.
  return BluetoothDevice::GetInquiryRSSI();
}

base::Optional<int8_t> BluetoothDeviceCast::GetInquiryTxPower() const {
  // TODO(slan): Remove if we do not need this.
  return BluetoothDevice::GetInquiryTxPower();
}

bool BluetoothDeviceCast::ExpectingPinCode() const {
  // TODO(slan): Implement this or rely on lower layers to do so.
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothDeviceCast::ExpectingPasskey() const {
  NOTIMPLEMENTED() << "Only BLE functionality is supported.";
  return false;
}

bool BluetoothDeviceCast::ExpectingConfirmation() const {
  NOTIMPLEMENTED() << "Only BLE functionality is supported.";
  return false;
}

void BluetoothDeviceCast::GetConnectionInfo(
    const ConnectionInfoCallback& callback) {
  // TODO(slan): Implement this?
  NOTIMPLEMENTED();
}

void BluetoothDeviceCast::SetConnectionLatency(
    ConnectionLatency connection_latency,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  // TODO(slan): This many be needed for some high-performance BLE devices.
  NOTIMPLEMENTED();
  error_callback.Run();
}

void BluetoothDeviceCast::Connect(PairingDelegate* pairing_delegate,
                                  const base::Closure& callback,
                                  const ConnectErrorCallback& error_callback) {
  // This method is used only for Bluetooth classic.
  NOTIMPLEMENTED() << __func__ << " Only BLE functionality is supported.";
  error_callback.Run(BluetoothDevice::ERROR_UNSUPPORTED_DEVICE);
}

void BluetoothDeviceCast::Pair(PairingDelegate* pairing_delegate,
                               const base::Closure& callback,
                               const ConnectErrorCallback& error_callback) {
  // TODO(slan): Implement this or delegate to lower level.
  NOTIMPLEMENTED();
  error_callback.Run(BluetoothDevice::ERROR_UNSUPPORTED_DEVICE);
}

void BluetoothDeviceCast::SetPinCode(const std::string& pincode) {
  NOTREACHED() << "Pairing not supported.";
}

void BluetoothDeviceCast::SetPasskey(uint32_t passkey) {
  NOTREACHED() << "Pairing not supported.";
}

void BluetoothDeviceCast::ConfirmPairing() {
  NOTREACHED() << "Pairing not supported.";
}

// Rejects a pairing or connection request from a remote device.
void BluetoothDeviceCast::RejectPairing() {
  NOTREACHED() << "Pairing not supported.";
}

void BluetoothDeviceCast::CancelPairing() {
  NOTREACHED() << "Pairing not supported.";
}

void BluetoothDeviceCast::Disconnect(const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
  // This method is used only for Bluetooth classic.
  NOTIMPLEMENTED() << __func__ << " Only BLE functionality is supported.";
  error_callback.Run();
}

void BluetoothDeviceCast::Forget(const base::Closure& callback,
                                 const ErrorCallback& error_callback) {
  NOTIMPLEMENTED() << __func__ << " Only BLE functionality is supported.";
  error_callback.Run();
}

void BluetoothDeviceCast::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  error_callback.Run("Not Implemented");
}

void BluetoothDeviceCast::ConnectToServiceInsecurely(
    const device::BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED() << __func__ << " GATT server mode not supported";
  error_callback.Run("Not Implemented");
}

bool BluetoothDeviceCast::UpdateWithScanResult(
    const chromecast::bluetooth::LeScanManager::ScanResult& result) {
  bool changed = false;

  // Advertisements for the same device can use different names. For now, the
  // last name wins.
  // TODO(slan): Make sure that this doesn't spam us with name changes.
  if (!name_ || result.name != *name_) {
    changed = true;
    name_ = result.name;
  }

  // Replace |device_uuids_| with newly advertised services. Currently this just
  // replaces them, but depending on what we see in the field, we may need to
  // take the union here instead. Note that this would require eviction of stale
  // services, preferably from the LeScanManager.
  // TODO(slan): Think about whether this is needed.
  UUIDSet prev_uuids = device_uuids_.GetUUIDs();
  UUIDSet new_uuids = ExtractServiceUuids(result, *GetName());
  if (prev_uuids != new_uuids) {
    device_uuids_.ReplaceAdvertisedUUIDs(
        UUIDList(new_uuids.begin(), new_uuids.end()));
    changed = true;
  }

  return changed;
}

bool BluetoothDeviceCast::SetConnected(bool connected) {
  DVLOG(2) << __func__ << " connected: " << connected;
  bool changed = false;
  if (!connected_ && connected) {
    DidConnectGatt();
    changed = true;
  } else if (connected_ && !connected) {
    DidDisconnectGatt();
    changed = true;
  }

  connected_ = connected;
  return changed;
}

void BluetoothDeviceCast::CreateGattConnectionImpl() {
  if (pending_connect_)
    return;
  pending_connect_ = true;
  remote_device_->Connect(base::BindOnce(&BluetoothDeviceCast::OnConnect,
                                         weak_factory_.GetWeakPtr()));
}

void BluetoothDeviceCast::DisconnectGatt() {
  if (pending_disconnect_)
    return;
  pending_disconnect_ = true;
  remote_device_->Disconnect(base::BindOnce(&BluetoothDeviceCast::OnDisconnect,
                                            weak_factory_.GetWeakPtr()));
}

void BluetoothDeviceCast::OnConnect(bool success) {
  pending_connect_ = false;
  if (success)
    SetConnected(true);
  else
    DidFailToConnectGatt(ERROR_FAILED);
}

void BluetoothDeviceCast::OnDisconnect(bool success) {
  pending_disconnect_ = false;
  if (success)
    SetConnected(false);
  else
    LOG(ERROR) << "Request to DisconnectGatt() failed!";
}

}  // namespace device
