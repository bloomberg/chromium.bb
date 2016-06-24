// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_BLUETOOTH_BLUETOOTH_TYPE_TRAITS_H_
#define COMPONENTS_ARC_BLUETOOTH_BLUETOOTH_TYPE_TRAITS_H_

#include "components/arc/common/bluetooth.mojom.h"
#include "device/bluetooth/bluetooth_common.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::BluetoothDeviceType,
    device::BluetoothTransport> {
  static arc::mojom::BluetoothDeviceType ToMojom(
      device::BluetoothTransport type) {
    switch (type) {
      case device::BLUETOOTH_TRANSPORT_CLASSIC:
        return arc::mojom::BluetoothDeviceType::BREDR;
      case device::BLUETOOTH_TRANSPORT_LE:
        return arc::mojom::BluetoothDeviceType::BLE;
      case device::BLUETOOTH_TRANSPORT_DUAL:
        return arc::mojom::BluetoothDeviceType::DUAL;
      default:
        NOTREACHED() << "Invalid type: " << static_cast<uint8_t>(type);
        // XXX: is there a better value to return here?
        return arc::mojom::BluetoothDeviceType::DUAL;
    }
  }

  static bool FromMojom(arc::mojom::BluetoothDeviceType mojom_type,
                        device::BluetoothTransport* type) {
    switch (mojom_type) {
      case arc::mojom::BluetoothDeviceType::BREDR:
        *type = device::BLUETOOTH_TRANSPORT_CLASSIC;
        break;
      case arc::mojom::BluetoothDeviceType::BLE:
        *type = device::BLUETOOTH_TRANSPORT_LE;
        break;
      case arc::mojom::BluetoothDeviceType::DUAL:
        *type = device::BLUETOOTH_TRANSPORT_DUAL;
        break;
      default:
        NOTREACHED() << "Invalid type: " << static_cast<uint32_t>(mojom_type);
        return false;
    }
    return true;
  }
};

}  // namespace mojo

#endif  // COMPONENTS_ARC_BLUETOOTH_BLUETOOTH_TYPE_CONVERTERS_H_
