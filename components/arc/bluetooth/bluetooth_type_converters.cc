// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/bluetooth/bluetooth_type_converters.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "mojo/public/cpp/bindings/array.h"

namespace {

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

// TODO(smbarber): Add unit tests for Bluetooth type converters.

// static
arc::mojom::BluetoothAddressPtr
TypeConverter<arc::mojom::BluetoothAddressPtr, std::string>::Convert(
    const std::string& address) {
  std::string stripped = StripNonHex(address);

  std::vector<uint8_t> address_bytes;
  base::HexStringToBytes(stripped, &address_bytes);

  arc::mojom::BluetoothAddressPtr mojo_addr =
      arc::mojom::BluetoothAddress::New();
  mojo_addr->address = mojo::Array<uint8_t>::From(address_bytes);

  return mojo_addr;
}

// static
std::string TypeConverter<std::string, arc::mojom::BluetoothAddress>::Convert(
    const arc::mojom::BluetoothAddress& address) {
  std::ostringstream addr_stream;
  addr_stream << std::setfill('0') << std::hex << std::uppercase;

  const mojo::Array<uint8_t>& bytes = address.address;

  for (size_t k = 0; k < bytes.size(); k++) {
    addr_stream << std::setw(2) << (unsigned int)bytes[k];
    addr_stream << ((k == bytes.size() - 1) ? "" : ":");
  }

  return addr_stream.str();
}

// static
arc::mojom::BluetoothUUIDPtr
TypeConverter<arc::mojom::BluetoothUUIDPtr, device::BluetoothUUID>::Convert(
    const device::BluetoothUUID& uuid) {
  std::string uuid_str = StripNonHex(uuid.canonical_value());

  std::vector<uint8_t> address_bytes;
  base::HexStringToBytes(uuid_str, &address_bytes);

  arc::mojom::BluetoothUUIDPtr uuidp = arc::mojom::BluetoothUUID::New();
  uuidp->uuid = mojo::Array<uint8_t>::From(address_bytes);

  return uuidp;
}

}  // namespace mojo
