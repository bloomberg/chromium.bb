// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/saved_devices_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_handle.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

using device::UsbDevice;
using device::UsbDeviceHandle;
using device::UsbEndpointDirection;
using device::UsbTransferCallback;
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
  MOCK_METHOD1(GetManufacturer, bool(base::string16*));
  MOCK_METHOD1(GetProduct, bool(base::string16*));

  bool GetSerialNumber(base::string16* serial) OVERRIDE {
    if (serial_number_.empty()) {
      return false;
    }

    *serial = base::UTF8ToUTF16(serial_number_);
    return true;
  }

  void NotifyDisconnect() { UsbDevice::NotifyDisconnect(); }

 private:
  virtual ~MockUsbDevice() {}

  const std::string serial_number_;
};
}

class SavedDevicesServiceTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
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
    service_ = SavedDevicesService::Get(env_.profile());
    device0 = new MockUsbDevice("ABCDE", 0);
    device1 = new MockUsbDevice("", 1);
    device2 = new MockUsbDevice("12345", 2);
    device3 = new MockUsbDevice("", 3);
  }

  extensions::TestExtensionEnvironment env_;
  const extensions::Extension* extension_;
  SavedDevicesService* service_;
  scoped_refptr<MockUsbDevice> device0;
  scoped_refptr<MockUsbDevice> device1;
  scoped_refptr<MockUsbDevice> device2;
  scoped_refptr<MockUsbDevice> device3;
};

TEST_F(SavedDevicesServiceTest, RegisterDevices) {
  SavedDevicesService::SavedDevices* saved_devices =
      service_->GetOrInsert(extension_->id());

  base::string16 serial_number(base::ASCIIToUTF16("ABCDE"));
  saved_devices->RegisterDevice(device0, &serial_number);
  saved_devices->RegisterDevice(device1, NULL);

  // This is necessary as writing out registered devices happens in a task on
  // the UI thread.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  ASSERT_TRUE(saved_devices->IsRegistered(device0));
  ASSERT_TRUE(saved_devices->IsRegistered(device1));
  ASSERT_FALSE(saved_devices->IsRegistered(device2));
  ASSERT_FALSE(saved_devices->IsRegistered(device3));

  std::vector<SavedDeviceEntry> device_entries =
      service_->GetAllDevices(extension_->id());
  ASSERT_EQ(1U, device_entries.size());
  ASSERT_EQ(base::ASCIIToUTF16("ABCDE"), device_entries[0].serial_number);

  device1->NotifyDisconnect();

  ASSERT_TRUE(saved_devices->IsRegistered(device0));
  ASSERT_FALSE(saved_devices->IsRegistered(device1));
  ASSERT_FALSE(saved_devices->IsRegistered(device2));
  ASSERT_FALSE(saved_devices->IsRegistered(device3));

  service_->Clear(extension_->id());

  // App is normally restarted, clearing its reference to the SavedDevices.
  saved_devices = service_->GetOrInsert(extension_->id());
  ASSERT_FALSE(saved_devices->IsRegistered(device0));
  device_entries = service_->GetAllDevices(extension_->id());
  ASSERT_EQ(0U, device_entries.size());
}

TEST_F(SavedDevicesServiceTest, LoadPrefs) {
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

  SavedDevicesService::SavedDevices* saved_devices =
      service_->GetOrInsert(extension_->id());
  ASSERT_TRUE(saved_devices->IsRegistered(device0));
  ASSERT_FALSE(saved_devices->IsRegistered(device1));
  ASSERT_FALSE(saved_devices->IsRegistered(device2));
  ASSERT_FALSE(saved_devices->IsRegistered(device3));
}

}  // namespace apps
