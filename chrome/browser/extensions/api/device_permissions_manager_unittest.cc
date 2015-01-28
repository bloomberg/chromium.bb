// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "device/core/device_client.h"
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
using device::UsbService;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

int next_id;

class MockUsbService : public UsbService {
 public:
  MockUsbService() : mock_device_client(this) {}

  MOCK_METHOD1(GetDeviceById, scoped_refptr<UsbDevice>(uint32));
  MOCK_METHOD1(GetDevices, void(std::vector<scoped_refptr<UsbDevice>>*));

  // Public wrapper for the protected NotifyDeviceRemove function.
  void NotifyDeviceRemoved(scoped_refptr<UsbDevice> device) {
    UsbService::NotifyDeviceRemoved(device);
  }

 private:
  class MockDeviceClient : device::DeviceClient {
   public:
    explicit MockDeviceClient(UsbService* usb_service)
        : usb_service_(usb_service) {}

    UsbService* GetUsbService() override { return usb_service_; }

   private:
    UsbService* usb_service_;
  };

  MockDeviceClient mock_device_client;
};

class MockUsbDevice : public UsbDevice {
 public:
  explicit MockUsbDevice(const std::string& serial_number)
      : UsbDevice(0, 0, next_id++) {
    if (serial_number.empty()) {
      EXPECT_CALL(*this, GetSerialNumber(_)).WillRepeatedly(Return(false));
    } else {
      EXPECT_CALL(*this, GetSerialNumber(_))
          .WillRepeatedly(
              DoAll(SetArgPointee<0>(base::ASCIIToUTF16(serial_number)),
                    Return(true)));
    }

    EXPECT_CALL(*this, GetProduct(_))
        .WillRepeatedly(
            DoAll(SetArgPointee<0>(base::ASCIIToUTF16("Test Product")),
                  Return(true)));
    EXPECT_CALL(*this, GetManufacturer(_))
        .WillRepeatedly(
            DoAll(SetArgPointee<0>(base::ASCIIToUTF16("Test Manufacturer")),
                  Return(true)));
  }

  MOCK_METHOD2(RequestUsbAccess, void(int, const base::Callback<void(bool)>&));
  MOCK_METHOD0(Open, scoped_refptr<UsbDeviceHandle>());
  MOCK_METHOD1(Close, bool(scoped_refptr<UsbDeviceHandle>));
  MOCK_METHOD0(GetConfiguration, const device::UsbConfigDescriptor*());
  MOCK_METHOD1(GetManufacturer, bool(base::string16*));
  MOCK_METHOD1(GetProduct, bool(base::string16*));
  MOCK_METHOD1(GetSerialNumber, bool(base::string16*));

 private:
  virtual ~MockUsbDevice() {}
};

void AllowUsbDevice(DevicePermissionsManager* manager,
                    const Extension* extension,
                    scoped_refptr<UsbDevice> device) {
  // If the device cannot provide any of these strings they will simply by
  // empty.
  base::string16 product;
  device->GetProduct(&product);
  base::string16 manufacturer;
  device->GetManufacturer(&manufacturer);
  base::string16 serial_number;
  device->GetSerialNumber(&serial_number);

  manager->AllowUsbDevice(
      extension->id(), device, product, manufacturer, serial_number);
}

scoped_refptr<DevicePermissionEntry> FindEntry(
    DevicePermissions* device_permissions,
    scoped_refptr<UsbDevice> device) {
  base::string16 serial_number;
  device->GetSerialNumber(&serial_number);

  return device_permissions->FindEntry(device, serial_number);
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
    device0_ = new MockUsbDevice("ABCDE");
    device1_ = new MockUsbDevice("");
    device2_ = new MockUsbDevice("12345");
    device3_ = new MockUsbDevice("");
    usb_service_ = new MockUsbService();
    UsbService::SetInstanceForTest(usb_service_);
  }

  extensions::TestExtensionEnvironment env_;
  const extensions::Extension* extension_;
  MockUsbService* usb_service_;
  scoped_refptr<MockUsbDevice> device0_;
  scoped_refptr<MockUsbDevice> device1_;
  scoped_refptr<MockUsbDevice> device2_;
  scoped_refptr<MockUsbDevice> device3_;
};

TEST_F(DevicePermissionsManagerTest, AllowAndClearDevices) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_.profile());
  AllowUsbDevice(manager, extension_, device0_);
  AllowUsbDevice(manager, extension_, device1_);

  scoped_ptr<DevicePermissions> device_permissions =
      manager->GetForExtension(extension_->id());
  scoped_refptr<DevicePermissionEntry> device0_entry =
      FindEntry(device_permissions.get(), device0_);
  ASSERT_TRUE(device0_entry.get());
  scoped_refptr<DevicePermissionEntry> device1_entry =
      FindEntry(device_permissions.get(), device1_);
  ASSERT_TRUE(device1_entry.get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device2_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device3_).get());
  ASSERT_EQ(2U, device_permissions->entries().size());

  ASSERT_EQ(base::ASCIIToUTF16(
                "Test Product from Test Manufacturer (serial number ABCDE)"),
            device0_entry->GetPermissionMessageString());
  ASSERT_EQ(base::ASCIIToUTF16("Test Product from Test Manufacturer"),
            device1_entry->GetPermissionMessageString());

  manager->Clear(extension_->id());

  device_permissions = manager->GetForExtension(extension_->id());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device0_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device1_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device2_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device3_).get());
  ASSERT_EQ(0U, device_permissions->entries().size());

  // After clearing device it should be possible to grant permission again.
  AllowUsbDevice(manager, extension_, device0_);
  AllowUsbDevice(manager, extension_, device1_);

  device_permissions = manager->GetForExtension(extension_->id());
  ASSERT_TRUE(FindEntry(device_permissions.get(), device0_).get());
  ASSERT_TRUE(FindEntry(device_permissions.get(), device1_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device2_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device3_).get());
}

TEST_F(DevicePermissionsManagerTest, SuspendExtension) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_.profile());
  AllowUsbDevice(manager, extension_, device0_);
  AllowUsbDevice(manager, extension_, device1_);

  scoped_ptr<DevicePermissions> device_permissions =
      manager->GetForExtension(extension_->id());
  ASSERT_TRUE(FindEntry(device_permissions.get(), device0_).get());
  ASSERT_TRUE(FindEntry(device_permissions.get(), device1_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device2_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device3_).get());

  manager->OnBackgroundHostClose(extension_->id());

  device_permissions = manager->GetForExtension(extension_->id());
  // Device 0 is still registered because its serial number has been stored in
  // ExtensionPrefs, it is "persistent".
  ASSERT_TRUE(FindEntry(device_permissions.get(), device0_).get());
  // Device 1 does not have uniquely identifying traits and so permission to
  // open it has been dropped when the app's windows have closed and the
  // background page has been suspended.
  ASSERT_FALSE(FindEntry(device_permissions.get(), device1_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device2_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device3_).get());
}

// TODO(reillyg): Until crbug.com/427985 is resolved device removal
// notifications are delivered asynchronously and so this test must be disabled.
TEST_F(DevicePermissionsManagerTest, DISABLED_DisconnectDevice) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_.profile());
  AllowUsbDevice(manager, extension_, device0_);
  AllowUsbDevice(manager, extension_, device1_);

  scoped_ptr<DevicePermissions> device_permissions =
      manager->GetForExtension(extension_->id());
  ASSERT_TRUE(FindEntry(device_permissions.get(), device0_).get());
  ASSERT_TRUE(FindEntry(device_permissions.get(), device1_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device2_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device3_).get());

  usb_service_->NotifyDeviceRemoved(device0_);
  usb_service_->NotifyDeviceRemoved(device1_);

  device_permissions = manager->GetForExtension(extension_->id());
  // Device 0 will be accessible when it is reconnected because it can be
  // recognized by its serial number.
  ASSERT_TRUE(FindEntry(device_permissions.get(), device0_).get());
  // Device 1 does not have a serial number and cannot be distinguished from
  // any other device of the same model so the app must request permission again
  // when it is reconnected.
  ASSERT_FALSE(FindEntry(device_permissions.get(), device1_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device2_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device3_).get());
}

TEST_F(DevicePermissionsManagerTest, RevokeAndRegrantAccess) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_.profile());
  AllowUsbDevice(manager, extension_, device0_);
  AllowUsbDevice(manager, extension_, device1_);

  scoped_ptr<DevicePermissions> device_permissions =
      manager->GetForExtension(extension_->id());
  scoped_refptr<DevicePermissionEntry> device0_entry =
      FindEntry(device_permissions.get(), device0_);
  ASSERT_TRUE(device0_entry.get());
  scoped_refptr<DevicePermissionEntry> device1_entry =
      FindEntry(device_permissions.get(), device1_);
  ASSERT_TRUE(device1_entry.get());

  manager->RemoveEntry(extension_->id(), device0_entry);
  device_permissions = manager->GetForExtension(extension_->id());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device0_).get());
  ASSERT_TRUE(FindEntry(device_permissions.get(), device1_).get());

  AllowUsbDevice(manager, extension_, device0_);
  device_permissions = manager->GetForExtension(extension_->id());
  ASSERT_TRUE(FindEntry(device_permissions.get(), device0_).get());
  ASSERT_TRUE(FindEntry(device_permissions.get(), device1_).get());

  manager->RemoveEntry(extension_->id(), device1_entry);
  device_permissions = manager->GetForExtension(extension_->id());
  ASSERT_TRUE(FindEntry(device_permissions.get(), device0_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device1_).get());

  AllowUsbDevice(manager, extension_, device1_);
  device_permissions = manager->GetForExtension(extension_->id());
  ASSERT_TRUE(FindEntry(device_permissions.get(), device0_).get());
  ASSERT_TRUE(FindEntry(device_permissions.get(), device1_).get());
}

TEST_F(DevicePermissionsManagerTest, UpdateLastUsed) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_.profile());
  AllowUsbDevice(manager, extension_, device0_);

  scoped_ptr<DevicePermissions> device_permissions =
      manager->GetForExtension(extension_->id());
  scoped_refptr<DevicePermissionEntry> device0_entry =
      FindEntry(device_permissions.get(), device0_);
  ASSERT_TRUE(device0_entry->last_used().is_null());

  manager->UpdateLastUsed(extension_->id(), device0_entry);
  device_permissions = manager->GetForExtension(extension_->id());
  device0_entry = FindEntry(device_permissions.get(), device0_);
  ASSERT_FALSE(device0_entry->last_used().is_null());
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
  env_.GetExtensionPrefs()->UpdateExtensionPref(extension_->id(), "devices",
                                                prefs_value.release());

  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_.profile());
  scoped_ptr<DevicePermissions> device_permissions =
      manager->GetForExtension(extension_->id());
  ASSERT_TRUE(FindEntry(device_permissions.get(), device0_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device1_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device2_).get());
  ASSERT_FALSE(FindEntry(device_permissions.get(), device3_).get());
}

}  // namespace extensions
