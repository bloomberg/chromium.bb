// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#endif

namespace device {

TEST(BluetoothDeviceTest, CanonicalizeAddressFormat_AcceptsAllValidFormats) {
  // There are three valid separators (':', '-', and none).
  // Case shouldn't matter.
  const char* const kValidFormats[] = {
    "1A:2B:3C:4D:5E:6F",
    "1a:2B:3c:4D:5e:6F",
    "1a:2b:3c:4d:5e:6f",
    "1A-2B-3C-4D-5E-6F",
    "1a-2B-3c-4D-5e-6F",
    "1a-2b-3c-4d-5e-6f",
    "1A2B3C4D5E6F",
    "1a2B3c4D5e6F",
    "1a2b3c4d5e6f",
  };

  for (size_t i = 0; i < arraysize(kValidFormats); ++i) {
    SCOPED_TRACE(std::string("Input format: '") + kValidFormats[i] + "'");
    EXPECT_EQ("1A:2B:3C:4D:5E:6F",
              BluetoothDevice::CanonicalizeAddress(kValidFormats[i]));
  }
}

TEST(BluetoothDeviceTest, CanonicalizeAddressFormat_RejectsInvalidFormats) {
  const char* const kValidFormats[] = {
    // Empty string.
    "",
    // Too short.
    "1A:2B:3C:4D:5E",
    // Too long.
    "1A:2B:3C:4D:5E:6F:70",
    // Missing a separator.
    "1A:2B:3C:4D:5E6F",
    // Mixed separators.
    "1A:2B-3C:4D-5E:6F",
    // Invalid characters.
    "1A:2B-3C:4D-5E:6X",
    // Separators in the wrong place.
    "1:A2:B3:C4:D5:E6F",
  };

  for (size_t i = 0; i < arraysize(kValidFormats); ++i) {
    SCOPED_TRACE(std::string("Input format: '") + kValidFormats[i] + "'");
    EXPECT_EQ(std::string(),
              BluetoothDevice::CanonicalizeAddress(kValidFormats[i]));
  }
}

#if defined(OS_ANDROID)
// Verifies basic device properties, e.g. GetAddress, GetName, ...
TEST_F(BluetoothTest, DeviceProperties) {
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  adapter_->StartDiscoverySession(GetDiscoverySessionCallback(),
                                  GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  DiscoverLowEnergyDevice(1);
  base::RunLoop().RunUntilIdle();
  BluetoothDevice* device = observer.last_device();
  ASSERT_TRUE(device);
  EXPECT_EQ(0x1F00u, device->GetBluetoothClass());
  EXPECT_EQ("AA:00:00:00:00:01", device->GetAddress());
  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());
  EXPECT_EQ(base::UTF8ToUTF16("FakeBluetoothDevice"), device->GetName());
  EXPECT_EQ(true, device->IsPaired());
  BluetoothDevice::UUIDList uuids = device->GetUUIDs();
  EXPECT_TRUE(ContainsValue(uuids, BluetoothUUID("1800")));
  EXPECT_TRUE(ContainsValue(uuids, BluetoothUUID("1801")));
}
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID)
// Device with no advertised Service UUIDs.
TEST_F(BluetoothTest, DeviceNoUUIDs) {
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  adapter_->StartDiscoverySession(GetDiscoverySessionCallback(),
                                  GetErrorCallback());
  base::RunLoop().RunUntilIdle();
  DiscoverLowEnergyDevice(3);
  base::RunLoop().RunUntilIdle();
  BluetoothDevice* device = observer.last_device();
  ASSERT_TRUE(device);
  BluetoothDevice::UUIDList uuids = device->GetUUIDs();
  EXPECT_EQ(0u, uuids.size());
}
#endif  // defined(OS_ANDROID)

// TODO(scheib): Test with a device with no name. http://crbug.com/506415
// BluetoothDevice::GetAddressWithLocalizedDeviceTypeName() will run, which
// requires string resources to be loaded. For that, something like
// InitSharedInstance must be run. See unittest files that call that. It will
// also require build configuration to generate string resources into a .pak
// file.

}  // namespace device
