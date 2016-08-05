// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/bluetooth/bluetooth_type_converters.h"

#include <string>
#include <vector>

#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "mojo/public/cpp/bindings/array.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kAddressStr[] = "1A:2B:3C:4D:5E:6F";
constexpr char kInvalidAddressStr[] = "00:00:00:00:00:00";
constexpr uint8_t kAddressArray[] = {0x1a, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f};
constexpr size_t kAddressSize = 6;
constexpr char kUuidStr[] = "12345678-1234-5678-9abc-def123456789";
constexpr uint8_t kUuidArray[] = {0x12, 0x34, 0x56, 0x78, 0x12, 0x34,
                                  0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf1,
                                  0x23, 0x45, 0x67, 0x89};
constexpr size_t kUuidSize = 16;
constexpr uint8_t kFillerByte = 0x79;
}  // namespace

namespace mojo {

TEST(BluetoothTypeConvertorTest, ConvertMojoBluetoothAddressFromString) {
  arc::mojom::BluetoothAddressPtr addressMojo =
      arc::mojom::BluetoothAddress::From(std::string(kAddressStr));
  EXPECT_EQ(kAddressSize, addressMojo->address.size());
  for (size_t i = 0; i < kAddressSize; i++) {
    EXPECT_EQ(kAddressArray[i], addressMojo->address[i]);
  }
}

TEST(BluetoothTypeConvertorTest, ConvertMojoBluetoothAddressToString) {
  arc::mojom::BluetoothAddressPtr addressMojo =
      arc::mojom::BluetoothAddress::New();
  for (size_t i = 0; i < kAddressSize - 1; i++) {
    addressMojo->address.push_back(kAddressArray[i]);
  }
  EXPECT_EQ(std::string(kInvalidAddressStr), addressMojo->To<std::string>());

  addressMojo->address.push_back(kAddressArray[kAddressSize - 1]);
  EXPECT_EQ(std::string(kAddressStr), addressMojo->To<std::string>());

  addressMojo->address.push_back(kFillerByte);

  EXPECT_EQ(std::string(kInvalidAddressStr), addressMojo->To<std::string>());
}

TEST(BluetoothTypeConvertorTest,
     ConvertMojoBluetoothUUIDFromDeviceBluetoothUUID) {
  device::BluetoothUUID uuidDevice((std::string(kUuidStr)));
  arc::mojom::BluetoothUUIDPtr uuidMojo =
      arc::mojom::BluetoothUUID::From(uuidDevice);
  EXPECT_EQ(kUuidSize, uuidMojo->uuid.size());
  for (size_t i = 0; i < kUuidSize; i++) {
    EXPECT_EQ(kUuidArray[i], uuidMojo->uuid[i]);
  }
}

TEST(BluetoothTypeConvertorTest,
     ConvertMojoBluetoothUUIDToDeviceBluetoothUUID) {
  arc::mojom::BluetoothUUIDPtr uuidMojo = arc::mojom::BluetoothUUID::New();
  for (size_t i = 0; i < kUuidSize - 1; i++) {
    uuidMojo->uuid.push_back(kUuidArray[i]);
  }
  EXPECT_FALSE(uuidMojo.To<device::BluetoothUUID>().IsValid());

  uuidMojo->uuid.push_back(kUuidArray[kUuidSize - 1]);
  EXPECT_TRUE(uuidMojo.To<device::BluetoothUUID>().IsValid());
  EXPECT_EQ(std::string(kUuidStr),
            uuidMojo.To<device::BluetoothUUID>().canonical_value());

  uuidMojo->uuid.push_back(kFillerByte);
  EXPECT_FALSE(uuidMojo.To<device::BluetoothUUID>().IsValid());
}

}  // namespace mojo
