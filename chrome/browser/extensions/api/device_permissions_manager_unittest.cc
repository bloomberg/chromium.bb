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

class MockDeviceClient : device::DeviceClient {
 public:
  MockDeviceClient() {}

  // device::DeviceClient implementation:
  UsbService* GetUsbService() override {
    DCHECK(usb_service_);
    return usb_service_;
  }

  void set_usb_service(UsbService* service) { usb_service_ = service; }

 private:
  UsbService* usb_service_ = nullptr;
};

class MockUsbService : public UsbService {
 public:
  MockUsbService() {}

  MOCK_METHOD1(GetDeviceById, scoped_refptr<UsbDevice>(uint32));
  MOCK_METHOD1(GetDevices, void(const GetDevicesCallback& callback));

  // Public wrapper for the protected NotifyDeviceRemove function.
  void NotifyDeviceRemoved(scoped_refptr<UsbDevice> device) {
    UsbService::NotifyDeviceRemoved(device);
  }
};

class MockUsbDevice : public UsbDevice {
 public:
  explicit MockUsbDevice(const std::string& serial_number)
      : UsbDevice(0,
                  0,
                  next_id++,
                  base::ASCIIToUTF16("Test Manufacturer"),
                  base::ASCIIToUTF16("Test Product"),
                  base::ASCIIToUTF16(serial_number)) {}

  MOCK_METHOD1(Open, void(const OpenCallback&));
  MOCK_METHOD1(Close, bool(scoped_refptr<UsbDeviceHandle>));
  MOCK_METHOD0(GetConfiguration, const device::UsbConfigDescriptor*());

 private:
  virtual ~MockUsbDevice() {}
};

}  // namespace

class DevicePermissionsManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();
    env_.reset(new extensions::TestExtensionEnvironment());
    env_->GetExtensionPrefs();  // Force creation before adding extensions.
    extension_ =
        env_->MakeExtension(*base::test::ParseJson(
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
    mock_device_client_.set_usb_service(&usb_service_);
  }

  void TearDown() override { env_.reset(nullptr); }

  scoped_ptr<extensions::TestExtensionEnvironment> env_;
  const extensions::Extension* extension_;
  MockDeviceClient mock_device_client_;
  MockUsbService usb_service_;
  scoped_refptr<MockUsbDevice> device0_;
  scoped_refptr<MockUsbDevice> device1_;
  scoped_refptr<MockUsbDevice> device2_;
  scoped_refptr<MockUsbDevice> device3_;
};

TEST_F(DevicePermissionsManagerTest, AllowAndClearDevices) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  manager->AllowUsbDevice(extension_->id(), device0_);
  manager->AllowUsbDevice(extension_->id(), device1_);

  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  scoped_refptr<DevicePermissionEntry> device0_entry =
      device_permissions->FindEntry(device0_);
  ASSERT_TRUE(device0_entry.get());
  scoped_refptr<DevicePermissionEntry> device1_entry =
      device_permissions->FindEntry(device1_);
  ASSERT_TRUE(device1_entry.get());
  ASSERT_FALSE(device_permissions->FindEntry(device2_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device3_).get());
  ASSERT_EQ(2U, device_permissions->entries().size());

  ASSERT_EQ(base::ASCIIToUTF16(
                "Test Product from Test Manufacturer (serial number ABCDE)"),
            device0_entry->GetPermissionMessageString());
  ASSERT_EQ(base::ASCIIToUTF16("Test Product from Test Manufacturer"),
            device1_entry->GetPermissionMessageString());

  manager->Clear(extension_->id());
  // The device_permissions object is deleted by Clear.
  device_permissions = manager->GetForExtension(extension_->id());

  ASSERT_FALSE(device_permissions->FindEntry(device0_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device1_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device2_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device3_).get());
  ASSERT_EQ(0U, device_permissions->entries().size());

  // After clearing device it should be possible to grant permission again.
  manager->AllowUsbDevice(extension_->id(), device0_);
  manager->AllowUsbDevice(extension_->id(), device1_);

  ASSERT_TRUE(device_permissions->FindEntry(device0_).get());
  ASSERT_TRUE(device_permissions->FindEntry(device1_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device2_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device3_).get());
}

TEST_F(DevicePermissionsManagerTest, SuspendExtension) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  manager->AllowUsbDevice(extension_->id(), device0_);
  manager->AllowUsbDevice(extension_->id(), device1_);

  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  ASSERT_TRUE(device_permissions->FindEntry(device0_).get());
  ASSERT_TRUE(device_permissions->FindEntry(device1_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device2_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device3_).get());

  manager->OnBackgroundHostClose(extension_->id());

  // Device 0 is still registered because its serial number has been stored in
  // ExtensionPrefs, it is "persistent".
  ASSERT_TRUE(device_permissions->FindEntry(device0_).get());
  // Device 1 does not have uniquely identifying traits and so permission to
  // open it has been dropped when the app's windows have closed and the
  // background page has been suspended.
  ASSERT_FALSE(device_permissions->FindEntry(device1_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device2_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device3_).get());
}

TEST_F(DevicePermissionsManagerTest, DisconnectDevice) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  manager->AllowUsbDevice(extension_->id(), device0_);
  manager->AllowUsbDevice(extension_->id(), device1_);

  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  ASSERT_TRUE(device_permissions->FindEntry(device0_).get());
  ASSERT_TRUE(device_permissions->FindEntry(device1_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device2_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device3_).get());

  usb_service_.NotifyDeviceRemoved(device0_);
  usb_service_.NotifyDeviceRemoved(device1_);

  // Device 0 will be accessible when it is reconnected because it can be
  // recognized by its serial number.
  ASSERT_TRUE(device_permissions->FindEntry(device0_).get());
  // Device 1 does not have a serial number and cannot be distinguished from
  // any other device of the same model so the app must request permission again
  // when it is reconnected.
  ASSERT_FALSE(device_permissions->FindEntry(device1_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device2_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device3_).get());
}

TEST_F(DevicePermissionsManagerTest, RevokeAndRegrantAccess) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  manager->AllowUsbDevice(extension_->id(), device0_);
  manager->AllowUsbDevice(extension_->id(), device1_);

  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  scoped_refptr<DevicePermissionEntry> device0_entry =
      device_permissions->FindEntry(device0_);
  ASSERT_TRUE(device0_entry.get());
  scoped_refptr<DevicePermissionEntry> device1_entry =
      device_permissions->FindEntry(device1_);
  ASSERT_TRUE(device1_entry.get());

  manager->RemoveEntry(extension_->id(), device0_entry);
  ASSERT_FALSE(device_permissions->FindEntry(device0_).get());
  ASSERT_TRUE(device_permissions->FindEntry(device1_).get());

  manager->AllowUsbDevice(extension_->id(), device0_);
  ASSERT_TRUE(device_permissions->FindEntry(device0_).get());
  ASSERT_TRUE(device_permissions->FindEntry(device1_).get());

  manager->RemoveEntry(extension_->id(), device1_entry);
  ASSERT_TRUE(device_permissions->FindEntry(device0_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device1_).get());

  manager->AllowUsbDevice(extension_->id(), device1_);
  ASSERT_TRUE(device_permissions->FindEntry(device0_).get());
  ASSERT_TRUE(device_permissions->FindEntry(device1_).get());
}

TEST_F(DevicePermissionsManagerTest, UpdateLastUsed) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  manager->AllowUsbDevice(extension_->id(), device0_);

  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  scoped_refptr<DevicePermissionEntry> device0_entry =
      device_permissions->FindEntry(device0_);
  ASSERT_TRUE(device0_entry->last_used().is_null());

  manager->UpdateLastUsed(extension_->id(), device0_entry);
  device0_entry = device_permissions->FindEntry(device0_);
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
  env_->GetExtensionPrefs()->UpdateExtensionPref(extension_->id(), "devices",
                                                 prefs_value.release());

  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  ASSERT_TRUE(device_permissions->FindEntry(device0_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device1_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device2_).get());
  ASSERT_FALSE(device_permissions->FindEntry(device3_).get());
}

}  // namespace extensions
