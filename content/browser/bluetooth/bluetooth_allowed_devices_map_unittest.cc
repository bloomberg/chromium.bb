// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_allowed_devices_map.h"

#include "base/strings/string_util.h"
#include "content/common/bluetooth/bluetooth_scan_filter.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using device::BluetoothUUID;

namespace content {
namespace {
const url::Origin kTestOrigin1(GURL("https://www.example1.com"));
const url::Origin kTestOrigin2(GURL("https://www.example2.com"));

const std::string kDeviceAddress1 = "00:00:00";
const std::string kDeviceAddress2 = "11:11:11";

const char kGlucoseUUIDString[] = "00001808-0000-1000-8000-00805f9b34fb";
const char kHeartRateUUIDString[] = "0000180d-0000-1000-8000-00805f9b34fb";
const char kBatteryServiceUUIDString[] = "0000180f-0000-1000-8000-00805f9b34fb";
const char kBloodPressureUUIDString[] = "00001813-0000-1000-8000-00805f9b34fb";
const char kCyclingPowerUUIDString[] = "00001818-0000-1000-8000-00805f9b34fb";
const BluetoothUUID kGlucoseUUID(kGlucoseUUIDString);
const BluetoothUUID kHeartRateUUID(kHeartRateUUIDString);
const BluetoothUUID kBatteryServiceUUID(kBatteryServiceUUIDString);
const BluetoothUUID kBloodPressureUUID(kBloodPressureUUIDString);
const BluetoothUUID kCyclingPowerUUID(kCyclingPowerUUIDString);

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

TEST(BluetoothAllowedDevicesMapTest, AllowedServices_OneOriginOneDevice) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  // Setup device.
  BluetoothScanFilter scanFilter1;
  scanFilter1.services.push_back(kGlucoseUUID);
  BluetoothScanFilter scanFilter2;
  scanFilter2.services.push_back(kHeartRateUUID);

  std::vector<BluetoothScanFilter> filters;
  filters.push_back(scanFilter1);
  filters.push_back(scanFilter2);

  std::vector<BluetoothUUID> optional_services;
  optional_services.push_back(kBatteryServiceUUID);
  optional_services.push_back(kHeartRateUUID);

  // Add to map.
  const std::string device_id1 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, filters, optional_services);

  // Access allowed services.
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kGlucoseUUIDString));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kHeartRateUUIDString));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBatteryServiceUUIDString));

  // Try to access a non-allowed service.
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBloodPressureUUIDString));

  // Try to access allowed services after removing device.
  allowed_devices_map.RemoveDevice(kTestOrigin1, kDeviceAddress1);

  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kGlucoseUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kHeartRateUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBatteryServiceUUIDString));

  // Add device back.
  const std::string device_id2 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, filters, kEmptyOptionalServices);

  // Access allowed services.
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kGlucoseUUIDString));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kHeartRateUUIDString));

  // Try to access a non-allowed service.
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kBatteryServiceUUIDString));

  // Try to access services from old device.
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kGlucoseUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kHeartRateUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBatteryServiceUUIDString));
}

TEST(BluetoothAllowedDevicesMapTest, AllowedServices_OneOriginTwoDevices) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  // Setup request for device #1.
  BluetoothScanFilter scanFilter1;
  scanFilter1.services.push_back(kGlucoseUUID);
  std::vector<BluetoothScanFilter> filters1;
  filters1.push_back(scanFilter1);

  std::vector<BluetoothUUID> optional_services1;
  optional_services1.push_back(kHeartRateUUID);

  // Setup request for device #2.
  BluetoothScanFilter scanFilter2;
  scanFilter2.services.push_back(kBatteryServiceUUID);
  std::vector<BluetoothScanFilter> filters2;
  filters2.push_back(scanFilter2);

  std::vector<BluetoothUUID> optional_services2;
  optional_services2.push_back(kBloodPressureUUID);

  // Add devices to map.
  const std::string& device_id1 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, filters1, optional_services1);
  const std::string& device_id2 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress2, filters2, optional_services2);

  // Access allowed services.
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kGlucoseUUIDString));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kHeartRateUUIDString));

  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kBatteryServiceUUIDString));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kBloodPressureUUIDString));

  // Try to access non-allowed services.
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBatteryServiceUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBloodPressureUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kCyclingPowerUUIDString));

  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kGlucoseUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kHeartRateUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kCyclingPowerUUIDString));
}

TEST(BluetoothAllowedDevicesMapTest, AllowedServices_TwoOriginsOneDevice) {
  BluetoothAllowedDevicesMap allowed_devices_map;
  // Setup request #1 for device.
  BluetoothScanFilter scanFilter1;
  scanFilter1.services.push_back(kGlucoseUUID);
  std::vector<BluetoothScanFilter> filters1;
  filters1.push_back(scanFilter1);

  std::vector<BluetoothUUID> optional_services1;
  optional_services1.push_back(kHeartRateUUID);

  // Setup request #2 for device.
  BluetoothScanFilter scanFilter2;
  scanFilter2.services.push_back(kBatteryServiceUUID);
  std::vector<BluetoothScanFilter> filters2;
  filters2.push_back(scanFilter2);

  std::vector<BluetoothUUID> optional_services2;
  optional_services2.push_back(kBloodPressureUUID);

  // Add devices to map.
  const std::string& device_id1 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, filters1, optional_services1);
  const std::string& device_id2 = allowed_devices_map.AddDevice(
      kTestOrigin2, kDeviceAddress1, filters2, optional_services2);

  // Access allowed services.
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kGlucoseUUIDString));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kHeartRateUUIDString));

  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id2, kBatteryServiceUUIDString));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id2, kBloodPressureUUIDString));

  // Try to access non-allowed services.
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBatteryServiceUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBloodPressureUUIDString));

  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kGlucoseUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kHeartRateUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kBatteryServiceUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id2, kBloodPressureUUIDString));

  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id2, kGlucoseUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id2, kHeartRateUUIDString));

  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id1, kGlucoseUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id1, kHeartRateUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id1, kBatteryServiceUUIDString));
  EXPECT_FALSE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin2, device_id1, kBloodPressureUUIDString));
}

TEST(BluetoothAllowedDevicesMapTest, MergeServices) {
  BluetoothAllowedDevicesMap allowed_devices_map;

  // Setup first request.
  BluetoothScanFilter scanFilter1;
  scanFilter1.services.push_back(kGlucoseUUID);
  std::vector<BluetoothScanFilter> filters1;
  filters1.push_back(scanFilter1);
  std::vector<BluetoothUUID> optional_services1;
  optional_services1.push_back(kBatteryServiceUUID);

  // Add to map.
  const std::string device_id1 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, filters1, optional_services1);

  // Setup second request.
  BluetoothScanFilter scanFilter2;
  scanFilter2.services.push_back(kHeartRateUUID);
  std::vector<BluetoothScanFilter> filters2;
  filters2.push_back(scanFilter2);
  std::vector<BluetoothUUID> optional_services2;
  optional_services2.push_back(kBloodPressureUUID);

  // Add to map again.
  const std::string device_id2 = allowed_devices_map.AddDevice(
      kTestOrigin1, kDeviceAddress1, filters2, optional_services2);

  EXPECT_EQ(device_id1, device_id2);

  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kGlucoseUUIDString));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBatteryServiceUUIDString));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kHeartRateUUIDString));
  EXPECT_TRUE(allowed_devices_map.IsOriginAllowedToAccessService(
      kTestOrigin1, device_id1, kBloodPressureUUIDString));
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
