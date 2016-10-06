// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/bluetooth/bluetooth_struct_traits.h"

#include "device/bluetooth/bluetooth_uuid.h"
#include "mojo/public/cpp/bindings/array.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kUuidStr[] = "12345678-1234-5678-9abc-def123456789";
constexpr uint8_t kUuidArray[] = {0x12, 0x34, 0x56, 0x78, 0x12, 0x34,
                                  0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf1,
                                  0x23, 0x45, 0x67, 0x89};
constexpr size_t kUuidSize = 16;

template <typename MojoType, typename UserType>
mojo::StructPtr<MojoType> ConvertToMojo(UserType& input) {
  mojo::Array<uint8_t> data = MojoType::Serialize(&input);
  mojo::StructPtr<MojoType> output;
  MojoType::Deserialize(std::move(data), &output);
  return output;
}

template <typename MojoType, typename UserType>
bool ConvertFromMojo(mojo::StructPtr<MojoType> input, UserType* output) {
  mojo::Array<uint8_t> data = MojoType::Serialize(&input);
  return MojoType::Deserialize(std::move(data), output);
}

}  // namespace

namespace mojo {

TEST(BluetoothStructTraitsTest, SerializeBluetoothUUID) {
  device::BluetoothUUID uuid_device(kUuidStr);
  arc::mojom::BluetoothUUIDPtr uuid_mojo =
      ConvertToMojo<arc::mojom::BluetoothUUID>(uuid_device);
  EXPECT_EQ(kUuidSize, uuid_mojo->uuid.size());
  for (size_t i = 0; i < kUuidSize; i++) {
    EXPECT_EQ(kUuidArray[i], uuid_mojo->uuid[i]);
  }
}

TEST(BluetoothStructTraitsTest, DeserializeBluetoothUUID) {
  arc::mojom::BluetoothUUIDPtr uuid_mojo = arc::mojom::BluetoothUUID::New();
  for (size_t i = 0; i < kUuidSize; i++) {
    uuid_mojo->uuid.push_back(kUuidArray[i]);
  }
  device::BluetoothUUID uuid_device;
  EXPECT_TRUE(ConvertFromMojo(std::move(uuid_mojo), &uuid_device));
  EXPECT_EQ(std::string(kUuidStr), uuid_device.canonical_value());

  // Size checks are performed in serialization code. UUIDs with a length
  // other than 16 bytes will not make it through the mojo deserialization
  // code since arc::mojom::BluetoothUUID::uuid is defined as
  // array<uint8, 16>.
}

}  // namespace mojo
