// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_CONNECT_RESULT_TYPE_CONVERTER_H_
#define DEVICE_BLUETOOTH_CONNECT_RESULT_TYPE_CONVERTER_H_

#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/interfaces/adapter.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

// TypeConverter to translate from
// device::BluetoothDevice::ConnectErrorCode to bluetooth.mojom.ConnectResult.
// TODO(crbug.com/666561): Replace because TypeConverter is deprecated.
template <>
struct TypeConverter<bluetooth::mojom::ConnectResult,
                     device::BluetoothDevice::ConnectErrorCode> {
  static bluetooth::mojom::ConnectResult Convert(
      const device::BluetoothDevice::ConnectErrorCode& input) {
    switch (input) {
      case device::BluetoothDevice::ConnectErrorCode::
          ERROR_ATTRIBUTE_LENGTH_INVALID:
        return bluetooth::mojom::ConnectResult::ATTRIBUTE_LENGTH_INVALID;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_CANCELED:
        return bluetooth::mojom::ConnectResult::AUTH_CANCELED;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_FAILED:
        return bluetooth::mojom::ConnectResult::AUTH_FAILED;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_REJECTED:
        return bluetooth::mojom::ConnectResult::AUTH_REJECTED;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_TIMEOUT:
        return bluetooth::mojom::ConnectResult::AUTH_TIMEOUT;
      case device::BluetoothDevice::ConnectErrorCode::
          ERROR_CONNECTION_CONGESTED:
        return bluetooth::mojom::ConnectResult::CONNECTION_CONGESTED;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_FAILED:
        return bluetooth::mojom::ConnectResult::FAILED;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_INPROGRESS:
        return bluetooth::mojom::ConnectResult::INPROGRESS;
      case device::BluetoothDevice::ConnectErrorCode::
          ERROR_INSUFFICIENT_ENCRYPTION:
        return bluetooth::mojom::ConnectResult::INSUFFICIENT_ENCRYPTION;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_OFFSET_INVALID:
        return bluetooth::mojom::ConnectResult::OFFSET_INVALID;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_READ_NOT_PERMITTED:
        return bluetooth::mojom::ConnectResult::READ_NOT_PERMITTED;
      case device::BluetoothDevice::ConnectErrorCode::
          ERROR_REQUEST_NOT_SUPPORTED:
        return bluetooth::mojom::ConnectResult::REQUEST_NOT_SUPPORTED;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN:
        return bluetooth::mojom::ConnectResult::UNKNOWN;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_UNSUPPORTED_DEVICE:
        return bluetooth::mojom::ConnectResult::UNSUPPORTED_DEVICE;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_WRITE_NOT_PERMITTED:
        return bluetooth::mojom::ConnectResult::WRITE_NOT_PERMITTED;
      case device::BluetoothDevice::ConnectErrorCode::NUM_CONNECT_ERROR_CODES:
        NOTREACHED();
        return bluetooth::mojom::ConnectResult::UNTRANSLATED_CONNECT_ERROR_CODE;
    }
    NOTREACHED();
    return bluetooth::mojom::ConnectResult::UNTRANSLATED_CONNECT_ERROR_CODE;
  }
};
}

#endif  // DEVICE_BLUETOOTH_CONNECT_RESULT_TYPE_CONVERTER_H_
