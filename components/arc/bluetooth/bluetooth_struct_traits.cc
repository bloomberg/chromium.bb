// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/bluetooth/bluetooth_struct_traits.h"

#include <algorithm>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace {

constexpr size_t kUUIDSize = 16;

bool IsNonHex(char c) {
  return !isxdigit(c);
}

std::string StripNonHex(const std::string& str) {
  std::string result = str;
  result.erase(std::remove_if(result.begin(), result.end(), IsNonHex),
               result.end());

  return result;
}

}  // namespace

namespace mojo {

// static
std::vector<uint8_t>
StructTraits<arc::mojom::BluetoothUUIDDataView, device::BluetoothUUID>::uuid(
    const device::BluetoothUUID& input) {
  std::string uuid_str = StripNonHex(input.canonical_value());

  std::vector<uint8_t> address_bytes;
  base::HexStringToBytes(uuid_str, &address_bytes);
  return address_bytes;
}

// static
bool StructTraits<arc::mojom::BluetoothUUIDDataView,
                  device::BluetoothUUID>::Read(
    arc::mojom::BluetoothUUIDDataView data,
    device::BluetoothUUID* output) {
  std::vector<uint8_t> address_bytes;
  data.ReadUuid(&address_bytes);

  if (address_bytes.size() != kUUIDSize)
    return false;

  // BluetoothUUID expects the format below with the dashes inserted.
  // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  std::string uuid_str =
      base::HexEncode(address_bytes.data(), address_bytes.size());
  constexpr size_t kUuidDashPos[] = {8, 13, 18, 23};
  for (auto pos : kUuidDashPos)
    uuid_str = uuid_str.insert(pos, "-");

  device::BluetoothUUID result(uuid_str);

  DCHECK(result.IsValid());
  *output = result;
  return true;
}

}  // namespace mojo
