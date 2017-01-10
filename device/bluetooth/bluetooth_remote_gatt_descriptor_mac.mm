// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_descriptor_mac.h"

#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_mac.h"

namespace device {

BluetoothRemoteGattDescriptorMac::BluetoothRemoteGattDescriptorMac(
    BluetoothRemoteGattCharacteristicMac* characteristic,
    CBDescriptor* descriptor)
    : gatt_characteristic_(characteristic),
      cb_descriptor_(descriptor, base::scoped_policy::RETAIN) {
  uuid_ =
      BluetoothAdapterMac::BluetoothUUIDWithCBUUID([cb_descriptor_.get() UUID]);
  identifier_ = base::SysNSStringToUTF8(
      [NSString stringWithFormat:@"%s-%p", uuid_.canonical_value().c_str(),
                                 (void*)cb_descriptor_]);
}

std::string BluetoothRemoteGattDescriptorMac::GetIdentifier() const {
  return identifier_;
}

BluetoothUUID BluetoothRemoteGattDescriptorMac::GetUUID() const {
  return uuid_;
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattDescriptorMac::GetPermissions() const {
  NOTIMPLEMENTED();
  return BluetoothGattCharacteristic::PERMISSION_NONE;
}

const std::vector<uint8_t>& BluetoothRemoteGattDescriptorMac::GetValue() const {
  return value_;
}

BluetoothRemoteGattDescriptorMac::~BluetoothRemoteGattDescriptorMac() {}

// Returns a pointer to the GATT characteristic that this characteristic
// descriptor belongs to.
BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattDescriptorMac::GetCharacteristic() const {
  return static_cast<BluetoothRemoteGattCharacteristic*>(gatt_characteristic_);
}

// Sends a read request to a remote characteristic descriptor to read its
// value. |callback| is called to return the read value on success and
// |error_callback| is called for failures.
void BluetoothRemoteGattDescriptorMac::ReadRemoteDescriptor(
    const ValueCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

// Sends a write request to a remote characteristic descriptor, to modify the
// value of the descriptor with the new value |new_value|. |callback| is
// called to signal success and |error_callback| for failures. This method
// only applies to remote descriptors and will fail for those that are locally
// hosted.
void BluetoothRemoteGattDescriptorMac::WriteRemoteDescriptor(
    const std::vector<uint8_t>& new_value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

CBDescriptor* BluetoothRemoteGattDescriptorMac::GetCBDescriptor() const {
  return cb_descriptor_.get();
}

}  // namespace device.
