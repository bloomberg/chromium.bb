// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_allowed_devices_map.h"

#include "base/strings/string_util.h"
#include "content/common/bluetooth/bluetooth_scan_filter.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {
const url::Origin kTestOrigin1(GURL("https://www.example1.com"));
const url::Origin kTestOrigin2(GURL("https://www.example2.com"));

const std::string kDeviceAddress1 = "00:00:00";
const std::string kDeviceAddress2 = "11:11:11";

const std::vector<content::BluetoothScanFilter> kEmptyFilters =
    std::vector<BluetoothScanFilter>();
const std::vector<device::BluetoothUUID> kEmptyOptionalServices =
    std::vector<device::BluetoothUUID>();
}  // namespace

TEST(BluetoothAllowedDevicesMapTest, AddDeviceToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  const std::string& device_id = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, kEmptyFilters, kEmptyOptionalServices);

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id,
            allowed_devices_map.GetDeviceId(kTestOrigin1, kDeviceAddress1));
  EXPECT_EQ(kDeviceAddress1,
            allowed_devices_map.GetDeviceAddress(kTestOrigin1, device_id));
}

TEST(BluetoothAllowedDevicesMapTest, AddDeviceToMapTwice) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string& device_id1 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, kEmptyFilters, kEmptyOptionalServices);
  const std::string& device_id2 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, kEmptyFilters, kEmptyOptionalServices);

  EXPECT_EQ(device_id1, device_id2);

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id1,
            allowed_devices_map.GetDeviceId(kTestOrigin1, kDeviceAddress1));
  EXPECT_EQ(kDeviceAddress1,
            allowed_devices_map.GetDeviceAddress(kTestOrigin1, device_id1));
}

TEST(BluetoothAllowedDevicesMapTest, AddTwoDevicesFromSameOriginToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string& device_id1 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, kEmptyFilters, kEmptyOptionalServices);
  const std::string& device_id2 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress2, kEmptyFilters, kEmptyOptionalServices);

  EXPECT_NE(device_id1, device_id2);

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id1,
            allowed_devices_map.GetDeviceId(kTestOrigin1, kDeviceAddress1));
  EXPECT_EQ(device_id2,
            allowed_devices_map.GetDeviceId(kTestOrigin1, kDeviceAddress2));

  EXPECT_EQ(kDeviceAddress1,
            allowed_devices_map.GetDeviceAddress(kTestOrigin1, device_id1));
  EXPECT_EQ(kDeviceAddress2,
            allowed_devices_map.GetDeviceAddress(kTestOrigin1, device_id2));
}

TEST(BluetoothAllowedDevicesMapTest, AddTwoDevicesFromTwoOriginsToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string& device_id1 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, kEmptyFilters, kEmptyOptionalServices);
  const std::string& device_id2 = allowed_devices_map.AddDevice(
      kTestOrigin2, kDeviceAddress2, kEmptyFilters, kEmptyOptionalServices);

  EXPECT_NE(device_id1, device_id2);

  // Test that the wrong origin doesn't have access to the device.

  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceId(kTestOrigin1, kDeviceAddress2));
  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceId(kTestOrigin2, kDeviceAddress1));

  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceAddress(kTestOrigin1, device_id2));
  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceAddress(kTestOrigin2, device_id1));

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id1,
            allowed_devices_map.GetDeviceId(kTestOrigin1, kDeviceAddress1));
  EXPECT_EQ(device_id2,
            allowed_devices_map.GetDeviceId(kTestOrigin2, kDeviceAddress2));

  EXPECT_EQ(kDeviceAddress1,
            allowed_devices_map.GetDeviceAddress(kTestOrigin1, device_id1));
  EXPECT_EQ(kDeviceAddress2,
            allowed_devices_map.GetDeviceAddress(kTestOrigin2, device_id2));
}

TEST(BluetoothAllowedDevicesMapTest, AddDeviceFromTwoOriginsToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string& device_id1 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, kEmptyFilters, kEmptyOptionalServices);
  const std::string& device_id2 = allowed_devices_map.AddDevice(
      kTestOrigin2, kDeviceAddress1, kEmptyFilters, kEmptyOptionalServices);

  EXPECT_NE(device_id1, device_id2);

  // Test that the wrong origin doesn't have access to the device.
  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceAddress(kTestOrigin1, device_id2));
  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceAddress(kTestOrigin2, device_id1));
}

TEST(BluetoothAllowedDevicesMapTest, AddRemoveAddDeviceToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string device_id_first_time = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, kEmptyFilters, kEmptyOptionalServices);

  allowed_devices_map.RemoveDevice(kTestOrigin1, kDeviceAddress1);

  const std::string device_id_second_time = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, kEmptyFilters, kEmptyOptionalServices);

  EXPECT_NE(device_id_first_time, device_id_second_time);
}

TEST(BluetoothAllowedDevicesMapTest, RemoveDeviceFromMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  const std::string& device_id = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, kEmptyFilters, kEmptyOptionalServices);

  allowed_devices_map.RemoveDevice(kTestOrigin1, kDeviceAddress1);

  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceId(kTestOrigin1, device_id));
  EXPECT_EQ(base::EmptyString(), allowed_devices_map.GetDeviceAddress(
                                     kTestOrigin1, kDeviceAddress1));
}

TEST(BluetoothAllowedDevicesMapTest, CorrectIdFormat) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  const std::string& device_id = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, kEmptyFilters, kEmptyOptionalServices);

  EXPECT_TRUE(device_id.size() == 24)
      << "Expected Lenghth of a 128bit string encoded to Base64.";
  EXPECT_TRUE((device_id[22] == '=') && (device_id[23] == '='))
      << "Expected padding characters for a 128bit string encoded to Base64.";
}

}  // namespace content
