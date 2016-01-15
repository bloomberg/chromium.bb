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
const url::Origin test_origin1(GURL("https://www.example1.com"));
const url::Origin test_origin2(GURL("https://www.example2.com"));

const std::string device_address1 = "00:00:00";
const std::string device_address2 = "11:11:11";

const std::vector<content::BluetoothScanFilter> filters =
    std::vector<BluetoothScanFilter>();
const std::vector<device::BluetoothUUID> optional_services =
    std::vector<device::BluetoothUUID>();
}  // namespace

class BluetoothAllowedDevicesMapTest : public testing::Test {};

TEST_F(BluetoothAllowedDevicesMapTest, AddDeviceToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  const std::string& device_id = allowed_devices_map.AddDevice(
      test_origin1, device_address1, filters, optional_services);

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id,
            allowed_devices_map.GetDeviceId(test_origin1, device_address1));
  EXPECT_EQ(device_address1,
            allowed_devices_map.GetDeviceAddress(test_origin1, device_id));
}

TEST_F(BluetoothAllowedDevicesMapTest, AddDeviceToMapTwice) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string& device_id1 = allowed_devices_map.AddDevice(
      test_origin1, device_address1, filters, optional_services);
  const std::string& device_id2 = allowed_devices_map.AddDevice(
      test_origin1, device_address1, filters, optional_services);

  EXPECT_EQ(device_id1, device_id2);

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id1,
            allowed_devices_map.GetDeviceId(test_origin1, device_address1));
  EXPECT_EQ(device_address1,
            allowed_devices_map.GetDeviceAddress(test_origin1, device_id1));
}

TEST_F(BluetoothAllowedDevicesMapTest, AddTwoDevicesFromSameOriginToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string& device_id1 = allowed_devices_map.AddDevice(
      test_origin1, device_address1, filters, optional_services);
  const std::string& device_id2 = allowed_devices_map.AddDevice(
      test_origin1, device_address2, filters, optional_services);

  EXPECT_NE(device_id1, device_id2);

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id1,
            allowed_devices_map.GetDeviceId(test_origin1, device_address1));
  EXPECT_EQ(device_id2,
            allowed_devices_map.GetDeviceId(test_origin1, device_address2));

  EXPECT_EQ(device_address1,
            allowed_devices_map.GetDeviceAddress(test_origin1, device_id1));
  EXPECT_EQ(device_address2,
            allowed_devices_map.GetDeviceAddress(test_origin1, device_id2));
}

TEST_F(BluetoothAllowedDevicesMapTest, AddTwoDevicesFromTwoOriginsToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string& device_id1 = allowed_devices_map.AddDevice(
      test_origin1, device_address1, filters, optional_services);
  const std::string& device_id2 = allowed_devices_map.AddDevice(
      test_origin2, device_address2, filters, optional_services);

  EXPECT_NE(device_id1, device_id2);

  // Test that the wrong origin doesn't have access to the device.

  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceId(test_origin1, device_address2));
  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceId(test_origin2, device_address1));

  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceAddress(test_origin1, device_id2));
  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceAddress(test_origin2, device_id1));

  // Test that we can retrieve the device address/id.
  EXPECT_EQ(device_id1,
            allowed_devices_map.GetDeviceId(test_origin1, device_address1));
  EXPECT_EQ(device_id2,
            allowed_devices_map.GetDeviceId(test_origin2, device_address2));

  EXPECT_EQ(device_address1,
            allowed_devices_map.GetDeviceAddress(test_origin1, device_id1));
  EXPECT_EQ(device_address2,
            allowed_devices_map.GetDeviceAddress(test_origin2, device_id2));
}

TEST_F(BluetoothAllowedDevicesMapTest, AddDeviceFromTwoOriginsToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string& device_id1 = allowed_devices_map.AddDevice(
      test_origin1, device_address1, filters, optional_services);
  const std::string& device_id2 = allowed_devices_map.AddDevice(
      test_origin2, device_address1, filters, optional_services);

  EXPECT_NE(device_id1, device_id2);

  // Test that the wrong origin doesn't have access to the device.
  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceAddress(test_origin1, device_id2));
  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceAddress(test_origin2, device_id1));
}

TEST_F(BluetoothAllowedDevicesMapTest, AddRemoveAddDeviceToMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  const std::string device_id_first_time = allowed_devices_map.AddDevice(
      test_origin1, device_address1, filters, optional_services);

  allowed_devices_map.RemoveDevice(test_origin1, device_address1);

  const std::string device_id_second_time = allowed_devices_map.AddDevice(
      test_origin1, device_address1, filters, optional_services);

  EXPECT_NE(device_id_first_time, device_id_second_time);
}

TEST_F(BluetoothAllowedDevicesMapTest, RemoveDeviceFromMap) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  const std::string& device_id = allowed_devices_map.AddDevice(
      test_origin1, device_address1, filters, optional_services);

  allowed_devices_map.RemoveDevice(test_origin1, device_address1);

  EXPECT_EQ(base::EmptyString(),
            allowed_devices_map.GetDeviceId(test_origin1, device_id));
  EXPECT_EQ(base::EmptyString(), allowed_devices_map.GetDeviceAddress(
                                     test_origin1, device_address1));
}

TEST_F(BluetoothAllowedDevicesMapTest, CorrectIdFormat) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  const std::string& device_id = allowed_devices_map.AddDevice(
      test_origin1, device_address1, filters, optional_services);

  EXPECT_TRUE(device_id.size() == 24)
      << "Expected Lenghth of a 128bit string encoded to Base64.";
  EXPECT_TRUE((device_id[22] == '=') && (device_id[23] == '='))
      << "Expected padding characters for a 128bit string encoded to Base64.";
}

}  // namespace content
