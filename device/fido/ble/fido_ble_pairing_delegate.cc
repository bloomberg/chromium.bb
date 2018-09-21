// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_pairing_delegate.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "device/fido/ble/fido_ble_device.h"

namespace device {

FidoBlePairingDelegate::FidoBlePairingDelegate() = default;

FidoBlePairingDelegate::~FidoBlePairingDelegate() = default;

void FidoBlePairingDelegate::RequestPinCode(device::BluetoothDevice* device) {
  auto it = bluetooth_device_pincode_map_.find(
      FidoBleDevice::GetId(device->GetAddress()));
  if (it == bluetooth_device_pincode_map_.end()) {
    device->CancelPairing();
    return;
  }

  device->SetPinCode(it->second);
}

void FidoBlePairingDelegate::RequestPasskey(device::BluetoothDevice* device) {
  auto it = bluetooth_device_pincode_map_.find(
      FidoBleDevice::GetId(device->GetAddress()));
  if (it == bluetooth_device_pincode_map_.end()) {
    device->CancelPairing();
    return;
  }

  uint32_t pass_key;
  if (!base::StringToUint(it->second, &pass_key)) {
    device->CancelPairing();
    return;
  }

  device->SetPasskey(pass_key);
}

void FidoBlePairingDelegate::DisplayPinCode(device::BluetoothDevice* device,
                                            const std::string& pincode) {
  NOTIMPLEMENTED();
}

void FidoBlePairingDelegate::DisplayPasskey(device::BluetoothDevice* device,
                                            uint32_t passkey) {
  NOTIMPLEMENTED();
}

void FidoBlePairingDelegate::KeysEntered(device::BluetoothDevice* device,
                                         uint32_t entered) {
  NOTIMPLEMENTED();
}

void FidoBlePairingDelegate::ConfirmPasskey(device::BluetoothDevice* device,
                                            uint32_t passkey) {
  NOTIMPLEMENTED();
  device->CancelPairing();
}

void FidoBlePairingDelegate::AuthorizePairing(device::BluetoothDevice* device) {
  NOTIMPLEMENTED();
  device->CancelPairing();
}

void FidoBlePairingDelegate::StoreBlePinCodeForDevice(
    std::string device_address,
    std::string pin_code) {
  bluetooth_device_pincode_map_.insert_or_assign(std::move(device_address),
                                                 std::move(pin_code));
}

}  // namespace device
