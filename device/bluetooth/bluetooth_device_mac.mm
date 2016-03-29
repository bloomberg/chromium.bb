// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_mac.h"

#include "device/bluetooth/bluetooth_adapter_mac.h"

static NSString* const kErrorDomain = @"ConnectErrorCode";

namespace device {

BluetoothDeviceMac::BluetoothDeviceMac(BluetoothAdapterMac* adapter)
    : BluetoothDevice(adapter) {}

BluetoothDeviceMac::~BluetoothDeviceMac() {
}

NSError* BluetoothDeviceMac::GetNSErrorFromConnectErrorCode(
    BluetoothDevice::ConnectErrorCode error_code) {
  // TODO(http://crbug.com/585894): Need to convert the error.
  return [NSError errorWithDomain:kErrorDomain code:error_code userInfo:nil];
}

BluetoothDevice::ConnectErrorCode
BluetoothDeviceMac::GetConnectErrorCodeFromNSError(NSError* error) {
  if ([error.domain isEqualToString:kErrorDomain]) {
    BluetoothDevice::ConnectErrorCode connect_error_code =
        (BluetoothDevice::ConnectErrorCode)error.code;
    if (connect_error_code >= 0 ||
        connect_error_code < BluetoothDevice::NUM_CONNECT_ERROR_CODES) {
      return connect_error_code;
    }
    DCHECK(false);
    return BluetoothDevice::ERROR_FAILED;
  }
  // TODO(http://crbug.com/585894): Need to convert the error.
  return BluetoothDevice::ERROR_FAILED;
}

}  // namespace device
