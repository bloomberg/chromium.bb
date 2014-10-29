// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_handle.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using device::UsbDevice;
using device::UsbDeviceHandle;
using testing::Return;

class MockUsbDevice : public UsbDevice {
 public:
  MockUsbDevice(const std::string& serial_number, uint32 unique_id)
      : UsbDevice(0, 0, unique_id), serial_number_(serial_number) {}

  MOCK_METHOD0(Open, scoped_refptr<UsbDeviceHandle>());
  MOCK_METHOD1(Close, bool(scoped_refptr<UsbDeviceHandle>));
#if defined(OS_CHROMEOS)
  MOCK_METHOD2(RequestUsbAccess, void(int, const base::Callback<void(bool)>&));
#endif
  MOCK_METHOD0(GetConfiguration, const device::UsbConfigDescriptor&());

  virtual bool GetManufacturer(base::string16* manufacturer) {
    *manufacturer = base::ASCIIToUTF16("Test Manufacturer");
    return true;
  }

  virtual bool GetProduct(base::string16* product) {
    *product = base::ASCIIToUTF16("Test Product");
    return true;
  }

  virtual bool GetSerialNumber(base::string16* serial_number) override {
    if (serial_number_.empty()) {
      return false;
    }

    *serial_number = base::UTF8ToUTF16(serial_number_);
    return true;
  }

  void NotifyDisconnect() { UsbDevice::NotifyDisconnect(); }

 private:
  virtual ~MockUsbDevice() {}

  const std::string serial_number_;
};

void AllowUsbDevice(DevicePermissionsManager* manager,
                    const Extension* extension,
                    scoped_refptr<UsbDevice> device) {
  base::string16 product;
  device->GetProduct(&product);
  base::string16 manufacturer;
  device->GetManufacturer(&manufacturer);
  base::string16 serial_number;
  device->GetSerialNumber(&serial_number);
  manager->AllowUsbDevice(
      extension->id(), device, product, manufacturer, serial_number);
}

}  // namespace

class DevicePermissionsManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();
    env_.GetExtensionPrefs();  // Force creation before adding extensions.
    extension_ = env_.MakeExtension(*base::test::ParseJson(
                                        "{"
                                        "  \"app\": {"
                                        "    \"background\": {"
                                        "      \"scripts\": [\"background.js\"]"
                                        "    }"
                                        "  },"
                                        "  \"permissions\": ["
                                        "    \"usb\""
                                        "  ]"
                                        "}"));
    device0 = new MockUsbDevice("ABCDE", 0);
    device1 = new MockUsbDevice("", 1);
    device2 = new MockUsbDevice("12345", 2);
    device3 = new MockUsbDevice("", 3);
  }

  extensions::TestExtensionEnvironment env_;
  const extensions::Extension* extension_;
  scoped_refptr<MockUsbDevice> device0;
  scoped_refptr<MockUsbDevice> device1;
  scoped_refptr<MockUsbDevice> device2;
  scoped_refptr<MockUsbDevice> device3;
};

TEST_F(DevicePermissionsManagerTest, RegisterDevices) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_.profile());
  AllowUsbDevice(manager, extension_, device0);
  AllowUsbDevice(manager, extension_, device1);

  scoped_ptr<DevicePermissions> device_permissions =
      manager->GetForExtension(extension_->id());
  ASSERT_TRUE(device_permissions->CheckUsbDevice(device0));
  ASSERT_TRUE(device_permissions->CheckUsbDevice(device1));
  ASSERT_FALSE(device_permissions->CheckUsbDevice(device2));
  ASSERT_FALSE(device_permissions->CheckUsbDevice(device3));

  std::vector<base::string16> device_messages =
      manager->GetPermissionMessageStrings(extension_->id());
  ASSERT_EQ(1U, device_messages.size());
  ASSERT_NE(device_messages[0].find(base::ASCIIToUTF16("ABCDE")),
            base::string16::npos);

  device1->NotifyDisconnect();

  device_permissions = manager->GetForExtension(extension_->id());
  ASSERT_TRUE(device_permissions->CheckUsbDevice(device0));
  ASSERT_FALSE(device_permissions->CheckUsbDevice(device1));
  ASSERT_FALSE(device_permissions->CheckUsbDevice(device2));
  ASSERT_FALSE(device_permissions->CheckUsbDevice(device3));

  manager->Clear(extension_->id());

  device_permissions = manager->GetForExtension(extension_->id());
  ASSERT_FALSE(device_permissions->CheckUsbDevice(device0));
  device_messages = manager->GetPermissionMessageStrings(extension_->id());
  ASSERT_EQ(0U, device_messages.size());
}

TEST_F(DevicePermissionsManagerTest, LoadPrefs) {
  scoped_ptr<base::Value> prefs_value = base::test::ParseJson(
      "["
      "  {"
      "    \"product_id\": 0,"
      "    \"serial_number\": \"ABCDE\","
      "    \"type\": \"usb\","
      "    \"vendor_id\": 0"
      "  }"
      "]");
  env_.GetExtensionPrefs()->UpdateExtensionPref(
      extension_->id(), "devices", prefs_value.release());

  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_.profile());
  scoped_ptr<DevicePermissions> device_permissions =
      manager->GetForExtension(extension_->id());
  ASSERT_TRUE(device_permissions->CheckUsbDevice(device0));
  ASSERT_FALSE(device_permissions->CheckUsbDevice(device1));
  ASSERT_FALSE(device_permissions->CheckUsbDevice(device2));
  ASSERT_FALSE(device_permissions->CheckUsbDevice(device3));
}

}  // namespace extensions
