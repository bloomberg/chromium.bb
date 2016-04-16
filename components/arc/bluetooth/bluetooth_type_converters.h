// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_BLUETOOTH_BLUETOOTH_TYPE_CONVERTERS_H_
#define COMPONENTS_ARC_BLUETOOTH_BLUETOOTH_TYPE_CONVERTERS_H_

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>

#include "components/arc/common/bluetooth.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace device {
class BluetoothUUID;
}

namespace mojo {

template <>
struct TypeConverter<arc::mojom::BluetoothAddressPtr, std::string> {
  static arc::mojom::BluetoothAddressPtr Convert(const std::string& address);
};

template <>
struct TypeConverter<std::string, arc::mojom::BluetoothAddress> {
  static std::string Convert(const arc::mojom::BluetoothAddress& ptr);
};

template <>
struct TypeConverter<arc::mojom::BluetoothUUIDPtr, device::BluetoothUUID> {
  static arc::mojom::BluetoothUUIDPtr Convert(
      const device::BluetoothUUID& uuid);
};

}  // namespace mojo

#endif  // COMPONENTS_ARC_BLUETOOTH_BLUETOOTH_TYPE_CONVERTERS_H_
