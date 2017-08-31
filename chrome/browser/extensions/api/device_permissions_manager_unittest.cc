// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "device/base/mock_device_client.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/mock_hid_service.h"
#include "device/hid/public/interfaces/hid.mojom.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/hid/hid_device_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using device::HidDeviceInfo;
using device::MockUsbDevice;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

#if defined(OS_MACOSX)
const uint64_t kTestDeviceIds[] = {1, 2, 3, 4};
#else
const char* kTestDeviceIds[] = {"A", "B", "C", "D"};
#endif

std::unique_ptr<KeyedService> CreateHidDeviceManager(
    content::BrowserContext* context) {
  return base::MakeUnique<HidDeviceManager>(context);
}

}  // namespace

class DevicePermissionsManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();
    env_.reset(new extensions::TestExtensionEnvironment());
    extension_ =
        env_->MakeExtension(*base::test::ParseJson(
                                "{"
                                "  \"app\": {"
                                "    \"background\": {"
                                "      \"scripts\": [\"background.js\"]"
                                "    }"
                                "  },"
                                "  \"permissions\": [ \"hid\", \"usb\" ]"
                                "}"));

    HidDeviceManager::GetFactoryInstance()->SetTestingFactory(
        env_->profile(), &CreateHidDeviceManager);
    device0_ =
        new MockUsbDevice(0, 0, "Test Manufacturer", "Test Product", "ABCDE");
    device1_ = new MockUsbDevice(0, 0, "Test Manufacturer", "Test Product", "");
    device2_ =
        new MockUsbDevice(0, 0, "Test Manufacturer", "Test Product", "12345");
    device3_ = new MockUsbDevice(0, 0, "Test Manufacturer", "Test Product", "");
    device4_ = new HidDeviceInfo(
        kTestDeviceIds[0], 0, 0, "Test HID Device", "abcde",
        device::mojom::HidBusType::kHIDBusTypeUSB, std::vector<uint8_t>());
    device_client_.hid_service()->AddDevice(device4_);
    device5_ = new HidDeviceInfo(kTestDeviceIds[1], 0, 0, "Test HID Device", "",
                                 device::mojom::HidBusType::kHIDBusTypeUSB,
                                 std::vector<uint8_t>());
    device_client_.hid_service()->AddDevice(device5_);
    device6_ = new HidDeviceInfo(
        kTestDeviceIds[2], 0, 0, "Test HID Device", "67890",
        device::mojom::HidBusType::kHIDBusTypeUSB, std::vector<uint8_t>());
    device_client_.hid_service()->AddDevice(device6_);
    device7_ = new HidDeviceInfo(kTestDeviceIds[3], 0, 0, "Test HID Device", "",
                                 device::mojom::HidBusType::kHIDBusTypeUSB,
                                 std::vector<uint8_t>());
    device_client_.hid_service()->AddDevice(device7_);
    device_client_.hid_service()->FirstEnumerationComplete();
  }

  void TearDown() override { env_.reset(nullptr); }

  std::unique_ptr<extensions::TestExtensionEnvironment> env_;
  const extensions::Extension* extension_;
  device::MockDeviceClient device_client_;
  scoped_refptr<MockUsbDevice> device0_;
  scoped_refptr<MockUsbDevice> device1_;
  scoped_refptr<MockUsbDevice> device2_;
  scoped_refptr<MockUsbDevice> device3_;
  scoped_refptr<HidDeviceInfo> device4_;
  scoped_refptr<HidDeviceInfo> device5_;
  scoped_refptr<HidDeviceInfo> device6_;
  scoped_refptr<HidDeviceInfo> device7_;
};

TEST_F(DevicePermissionsManagerTest, AllowAndClearDevices) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  manager->AllowUsbDevice(extension_->id(), device0_);
  manager->AllowUsbDevice(extension_->id(), device1_);
  manager->AllowHidDevice(extension_->id(), *device4_->device());
  manager->AllowHidDevice(extension_->id(), *device5_->device());

  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  scoped_refptr<DevicePermissionEntry> device0_entry =
      device_permissions->FindUsbDeviceEntry(device0_);
  ASSERT_TRUE(device0_entry.get());
  scoped_refptr<DevicePermissionEntry> device1_entry =
      device_permissions->FindUsbDeviceEntry(device1_);
  ASSERT_TRUE(device1_entry.get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device2_).get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device3_).get());
  scoped_refptr<DevicePermissionEntry> device4_entry =
      device_permissions->FindHidDeviceEntry(*device4_->device());
  ASSERT_TRUE(device4_entry.get());
  scoped_refptr<DevicePermissionEntry> device5_entry =
      device_permissions->FindHidDeviceEntry(*device5_->device());
  ASSERT_TRUE(device5_entry.get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device6_->device()).get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device7_->device()).get());
  EXPECT_EQ(4U, device_permissions->entries().size());

  EXPECT_EQ(base::ASCIIToUTF16(
                "Test Product from Test Manufacturer (serial number ABCDE)"),
            device0_entry->GetPermissionMessageString());
  EXPECT_EQ(base::ASCIIToUTF16("Test Product from Test Manufacturer"),
            device1_entry->GetPermissionMessageString());
  EXPECT_EQ(base::ASCIIToUTF16("Test HID Device (serial number abcde)"),
            device4_entry->GetPermissionMessageString());
  EXPECT_EQ(base::ASCIIToUTF16("Test HID Device"),
            device5_entry->GetPermissionMessageString());

  manager->Clear(extension_->id());
  // The device_permissions object is deleted by Clear.
  device_permissions = manager->GetForExtension(extension_->id());

  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device0_).get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device1_).get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device2_).get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device3_).get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device4_->device()).get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device5_->device()).get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device6_->device()).get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device7_->device()).get());
  EXPECT_EQ(0U, device_permissions->entries().size());

  // After clearing device it should be possible to grant permission again.
  manager->AllowUsbDevice(extension_->id(), device0_);
  manager->AllowUsbDevice(extension_->id(), device1_);
  manager->AllowHidDevice(extension_->id(), *device4_->device());
  manager->AllowHidDevice(extension_->id(), *device5_->device());

  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(device0_).get());
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(device1_).get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device2_).get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device3_).get());
  EXPECT_TRUE(
      device_permissions->FindHidDeviceEntry(*device4_->device()).get());
  EXPECT_TRUE(
      device_permissions->FindHidDeviceEntry(*device5_->device()).get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device6_->device()).get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device7_->device()).get());
}

TEST_F(DevicePermissionsManagerTest, DisconnectDevice) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  manager->AllowUsbDevice(extension_->id(), device0_);
  manager->AllowUsbDevice(extension_->id(), device1_);
  manager->AllowHidDevice(extension_->id(), *device4_->device());
  manager->AllowHidDevice(extension_->id(), *device5_->device());

  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(device0_).get());
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(device1_).get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device2_).get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device3_).get());
  EXPECT_TRUE(
      device_permissions->FindHidDeviceEntry(*device4_->device()).get());
  EXPECT_TRUE(
      device_permissions->FindHidDeviceEntry(*device5_->device()).get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device6_->device()).get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device7_->device()).get());

  device_client_.usb_service()->RemoveDevice(device0_);
  device_client_.usb_service()->RemoveDevice(device1_);

  // Wait until HidDeviceManager::GetDevicesCallback is run. HidService
  // won't send notifications to its observers before that.
  base::RunLoop().RunUntilIdle();
  device_client_.hid_service()->RemoveDevice(device4_->platform_device_id());
  device_client_.hid_service()->RemoveDevice(device5_->platform_device_id());

  // Device 0 will be accessible when it is reconnected because it can be
  // recognized by its serial number.
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(device0_).get());
  // Device 1 does not have a serial number and cannot be distinguished from
  // any other device of the same model so the app must request permission again
  // when it is reconnected.
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device1_).get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device2_).get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device3_).get());
  // Device 4 is like device 0, but HID.
  EXPECT_TRUE(
      device_permissions->FindHidDeviceEntry(*device4_->device()).get());
  // Device 5 is like device 1, but HID.
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device5_->device()).get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device6_->device()).get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device7_->device()).get());
}

TEST_F(DevicePermissionsManagerTest, RevokeAndRegrantAccess) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  manager->AllowUsbDevice(extension_->id(), device0_);
  manager->AllowUsbDevice(extension_->id(), device1_);
  manager->AllowHidDevice(extension_->id(), *device4_->device());
  manager->AllowHidDevice(extension_->id(), *device5_->device());

  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  scoped_refptr<DevicePermissionEntry> device0_entry =
      device_permissions->FindUsbDeviceEntry(device0_);
  ASSERT_TRUE(device0_entry.get());
  scoped_refptr<DevicePermissionEntry> device1_entry =
      device_permissions->FindUsbDeviceEntry(device1_);
  ASSERT_TRUE(device1_entry.get());
  scoped_refptr<DevicePermissionEntry> device4_entry =
      device_permissions->FindHidDeviceEntry(*device4_->device());
  ASSERT_TRUE(device4_entry.get());
  scoped_refptr<DevicePermissionEntry> device5_entry =
      device_permissions->FindHidDeviceEntry(*device5_->device());
  ASSERT_TRUE(device5_entry.get());

  manager->RemoveEntry(extension_->id(), device0_entry);
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device0_).get());
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(device1_).get());

  manager->AllowUsbDevice(extension_->id(), device0_);
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(device0_).get());
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(device1_).get());

  manager->RemoveEntry(extension_->id(), device1_entry);
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(device0_).get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device1_).get());

  manager->AllowUsbDevice(extension_->id(), device1_);
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(device0_).get());
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(device1_).get());

  manager->RemoveEntry(extension_->id(), device4_entry);
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device4_->device()).get());
  EXPECT_TRUE(
      device_permissions->FindHidDeviceEntry(*device5_->device()).get());

  manager->AllowHidDevice(extension_->id(), *device4_->device());
  EXPECT_TRUE(
      device_permissions->FindHidDeviceEntry(*device4_->device()).get());
  EXPECT_TRUE(
      device_permissions->FindHidDeviceEntry(*device5_->device()).get());

  manager->RemoveEntry(extension_->id(), device5_entry);
  EXPECT_TRUE(
      device_permissions->FindHidDeviceEntry(*device4_->device()).get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device5_->device()).get());

  manager->AllowHidDevice(extension_->id(), *device5_->device());
  EXPECT_TRUE(
      device_permissions->FindHidDeviceEntry(*device4_->device()).get());
  EXPECT_TRUE(
      device_permissions->FindHidDeviceEntry(*device5_->device()).get());
}

TEST_F(DevicePermissionsManagerTest, UpdateLastUsed) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  manager->AllowUsbDevice(extension_->id(), device0_);
  manager->AllowHidDevice(extension_->id(), *device4_->device());

  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  scoped_refptr<DevicePermissionEntry> device0_entry =
      device_permissions->FindUsbDeviceEntry(device0_);
  EXPECT_TRUE(device0_entry->last_used().is_null());
  scoped_refptr<DevicePermissionEntry> device4_entry =
      device_permissions->FindHidDeviceEntry(*device4_->device());
  EXPECT_TRUE(device4_entry->last_used().is_null());

  manager->UpdateLastUsed(extension_->id(), device0_entry);
  EXPECT_FALSE(device0_entry->last_used().is_null());
  manager->UpdateLastUsed(extension_->id(), device4_entry);
  EXPECT_FALSE(device4_entry->last_used().is_null());
}

TEST_F(DevicePermissionsManagerTest, LoadPrefs) {
  std::unique_ptr<base::Value> prefs_value = base::test::ParseJson(
      "["
      "  {"
      "    \"manufacturer_string\": \"Test Manufacturer\","
      "    \"product_id\": 0,"
      "    \"product_string\": \"Test Product\","
      "    \"serial_number\": \"ABCDE\","
      "    \"type\": \"usb\","
      "    \"vendor_id\": 0"
      "  },"
      "  {"
      "    \"product_id\": 0,"
      "    \"product_string\": \"Test HID Device\","
      "    \"serial_number\": \"abcde\","
      "    \"type\": \"hid\","
      "    \"vendor_id\": 0"
      "  }"
      "]");
  env_->GetExtensionPrefs()->UpdateExtensionPref(extension_->id(), "devices",
                                                 std::move(prefs_value));

  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  scoped_refptr<DevicePermissionEntry> device0_entry =
      device_permissions->FindUsbDeviceEntry(device0_);
  ASSERT_TRUE(device0_entry.get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device1_).get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device2_).get());
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(device3_).get());
  scoped_refptr<DevicePermissionEntry> device4_entry =
      device_permissions->FindHidDeviceEntry(*device4_->device());
  ASSERT_TRUE(device4_entry.get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device5_->device()).get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device6_->device()).get());
  EXPECT_FALSE(
      device_permissions->FindHidDeviceEntry(*device7_->device()).get());

  EXPECT_EQ(base::ASCIIToUTF16(
                "Test Product from Test Manufacturer (serial number ABCDE)"),
            device0_entry->GetPermissionMessageString());
  EXPECT_EQ(base::ASCIIToUTF16("Test HID Device (serial number abcde)"),
            device4_entry->GetPermissionMessageString());
}

TEST_F(DevicePermissionsManagerTest, PermissionMessages) {
  base::string16 empty;
  base::string16 product(base::ASCIIToUTF16("Widget"));
  base::string16 manufacturer(base::ASCIIToUTF16("ACME"));
  base::string16 serial_number(base::ASCIIToUTF16("A"));

  EXPECT_EQ(base::ASCIIToUTF16("Unknown product 0001 from vendor 0000"),
            DevicePermissionsManager::GetPermissionMessage(
                0x0000, 0x0001, empty, empty, empty, false));

  EXPECT_EQ(base::ASCIIToUTF16(
                "Unknown product 0001 from vendor 0000 (serial number A)"),
            DevicePermissionsManager::GetPermissionMessage(
                0x0000, 0x0001, empty, empty, base::ASCIIToUTF16("A"), false));

  EXPECT_EQ(base::ASCIIToUTF16("Unknown product 0001 from Google Inc."),
            DevicePermissionsManager::GetPermissionMessage(
                0x18D1, 0x0001, empty, empty, empty, false));

  EXPECT_EQ(base::ASCIIToUTF16(
                "Unknown product 0001 from Google Inc. (serial number A)"),
            DevicePermissionsManager::GetPermissionMessage(
                0x18D1, 0x0001, empty, empty, serial_number, false));

  EXPECT_EQ(base::ASCIIToUTF16("Nexus One from Google Inc."),
            DevicePermissionsManager::GetPermissionMessage(
                0x18D1, 0x4E11, empty, empty, empty, true));

  EXPECT_EQ(base::ASCIIToUTF16("Nexus One from Google Inc. (serial number A)"),
            DevicePermissionsManager::GetPermissionMessage(
                0x18D1, 0x4E11, empty, empty, serial_number, true));

  EXPECT_EQ(base::ASCIIToUTF16("Nexus One"),
            DevicePermissionsManager::GetPermissionMessage(
                0x18D1, 0x4E11, empty, empty, empty, false));

  EXPECT_EQ(base::ASCIIToUTF16("Nexus One (serial number A)"),
            DevicePermissionsManager::GetPermissionMessage(
                0x18D1, 0x4E11, empty, empty, serial_number, false));

  EXPECT_EQ(base::ASCIIToUTF16("Unknown product 0001 from ACME"),
            DevicePermissionsManager::GetPermissionMessage(
                0x0000, 0x0001, manufacturer, empty, empty, false));

  EXPECT_EQ(
      base::ASCIIToUTF16("Unknown product 0001 from ACME (serial number A)"),
      DevicePermissionsManager::GetPermissionMessage(
          0x0000, 0x0001, manufacturer, empty, serial_number, false));

  EXPECT_EQ(base::ASCIIToUTF16("Widget from ACME"),
            DevicePermissionsManager::GetPermissionMessage(
                0x0001, 0x0000, manufacturer, product, empty, true));

  EXPECT_EQ(base::ASCIIToUTF16("Widget from ACME (serial number A)"),
            DevicePermissionsManager::GetPermissionMessage(
                0x0001, 0x0000, manufacturer, product, serial_number, true));

  EXPECT_EQ(base::ASCIIToUTF16("Widget"),
            DevicePermissionsManager::GetPermissionMessage(
                0x0001, 0x0000, manufacturer, product, empty, false));

  EXPECT_EQ(base::ASCIIToUTF16("Widget (serial number A)"),
            DevicePermissionsManager::GetPermissionMessage(
                0x0001, 0x0000, manufacturer, product, serial_number, false));
}

}  // namespace extensions
