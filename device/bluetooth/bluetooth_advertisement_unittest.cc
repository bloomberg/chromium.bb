// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_advertisement.h"

#include <stdint.h>

#include <memory>

#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

TEST(BluetoothAdvertisementTest, DataMembersAreAssignedCorrectly) {
  // Sample manufacturer data.
  BluetoothAdvertisement::ManufacturerData manufacturer_data;
  std::vector<uint8_t> sample_data(5, 0);
  manufacturer_data[0] = std::vector<uint8_t>(5, 0);
  // Sample UUID List.
  const BluetoothAdvertisement::UUIDList uuids(1, "1234");
  // Sample Service Data.
  BluetoothAdvertisement::ServiceData service_data;
  service_data["1234"] = std::vector<uint8_t>(5, 0);

  BluetoothAdvertisement::Data data(
      BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);
  ASSERT_EQ(data.type(), BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);

  // Try without assiging Service UUID.
  ASSERT_FALSE(data.service_uuids().get());
  // Assign Service UUID.
  data.set_service_uuids(
      std::make_unique<BluetoothAdvertisement::UUIDList>(uuids));
  // Retrieve Service UUID.
  ASSERT_EQ(*data.service_uuids(), uuids);
  // Retrieve again.
  ASSERT_FALSE(data.service_uuids().get());

  // Try without assigning Manufacturer Data.
  ASSERT_FALSE(data.manufacturer_data().get());
  // Assign Manufacturer Data.
  data.set_manufacturer_data(
      std::make_unique<BluetoothAdvertisement::ManufacturerData>(
          manufacturer_data));
  // Retrieve Manufacturer Data.
  ASSERT_EQ(*data.manufacturer_data(), manufacturer_data);
  // Retrieve again.
  ASSERT_FALSE(data.manufacturer_data().get());

  // Try without assigning Solicit UUIDs.
  ASSERT_FALSE(data.solicit_uuids().get());
  // Assign Solicit UUIDs.
  data.set_solicit_uuids(
      std::make_unique<BluetoothAdvertisement::UUIDList>(uuids));
  // Retrieve Solicit UUIDs.
  ASSERT_EQ(*data.solicit_uuids(), uuids);
  // Retieve again.
  ASSERT_FALSE(data.solicit_uuids().get());

  // Try without assigning Service Data.
  ASSERT_FALSE(data.service_data().get());
  // Assign Service Data.
  data.set_service_data(
      std::make_unique<BluetoothAdvertisement::ServiceData>(service_data));
  // Retrieve Service Data.
  ASSERT_EQ(*data.service_data(), service_data);
  // Retrieve again.
  ASSERT_FALSE(data.service_data().get());
}

}  // namespace

}  // namespace device
