// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "device/base/mock_device_client.h"
#include "device/hid/hid_collection_info.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_usage_and_page.h"
#include "device/hid/mock_hid_service.h"
#include "device/hid/public/interfaces/hid.mojom.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/shell/browser/shell_extensions_api_client.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/base/io_buffer.h"

using base::ThreadTaskRunnerHandle;
using device::HidCollectionInfo;
using device::HidDeviceInfo;
using device::HidPlatformDeviceId;
using device::HidUsageAndPage;
using device::MockDeviceClient;
using net::IOBuffer;
using testing::_;

#if defined(OS_MACOSX)
const uint64_t kTestDeviceIds[] = {1, 2, 3, 4, 5};
#else
const char* const kTestDeviceIds[] = {"A", "B", "C", "D", "E"};
#endif

// These report descriptors define two devices with 8-byte input, output and
// feature reports. The first implements usage page 0xFF00 and has a single
// report without and ID. The second implements usage page 0xFF01 and has a
// single report with ID 1.
const uint8_t kReportDescriptor[] = {0x06, 0x00, 0xFF, 0x08, 0xA1, 0x01, 0x15,
                                     0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95,
                                     0x08, 0x08, 0x81, 0x02, 0x08, 0x91, 0x02,
                                     0x08, 0xB1, 0x02, 0xC0};
const uint8_t kReportDescriptorWithIDs[] = {
    0x06, 0x01, 0xFF, 0x08, 0xA1, 0x01, 0x15, 0x00, 0x26,
    0xFF, 0x00, 0x85, 0x01, 0x75, 0x08, 0x95, 0x08, 0x08,
    0x81, 0x02, 0x08, 0x91, 0x02, 0x08, 0xB1, 0x02, 0xC0};

namespace extensions {

class MockHidConnection : public device::HidConnection {
 public:
  explicit MockHidConnection(scoped_refptr<HidDeviceInfo> device_info)
      : HidConnection(device_info) {}

  void PlatformClose() override {}

  void PlatformRead(ReadCallback callback) override {
    const char kResult[] = "This is a HID input report.";
    uint8_t report_id = device_info()->has_report_id() ? 1 : 0;
    scoped_refptr<IOBuffer> buffer(new IOBuffer(sizeof(kResult)));
    buffer->data()[0] = report_id;
    memcpy(buffer->data() + 1, kResult, sizeof(kResult) - 1);
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), true, buffer, sizeof(kResult)));
  }

  void PlatformWrite(scoped_refptr<net::IOBuffer> buffer,
                     size_t size,
                     WriteCallback callback) override {
    const char kExpected[] = "o-report";  // 8 bytes
    bool result = false;
    if (size == sizeof(kExpected)) {
      uint8_t report_id = buffer->data()[0];
      uint8_t expected_report_id = device_info()->has_report_id() ? 1 : 0;
      if (report_id == expected_report_id) {
        if (memcmp(buffer->data() + 1, kExpected, sizeof(kExpected) - 1) == 0) {
          result = true;
        }
      }
    }
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  }

  void PlatformGetFeatureReport(uint8_t report_id,
                                ReadCallback callback) override {
    const char kResult[] = "This is a HID feature report.";
    scoped_refptr<IOBuffer> buffer(new IOBuffer(sizeof(kResult)));
    size_t offset = 0;
    if (device_info()->has_report_id()) {
      buffer->data()[offset++] = report_id;
    }
    memcpy(buffer->data() + offset, kResult, sizeof(kResult) - 1);
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true, buffer,
                                  sizeof(kResult) - 1 + offset));
  }

  void PlatformSendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                 size_t size,
                                 WriteCallback callback) override {
    const char kExpected[] = "The app is setting this HID feature report.";
    bool result = false;
    if (size == sizeof(kExpected)) {
      uint8_t report_id = buffer->data()[0];
      uint8_t expected_report_id = device_info()->has_report_id() ? 1 : 0;
      if (report_id == expected_report_id &&
          memcmp(buffer->data() + 1, kExpected, sizeof(kExpected) - 1) == 0) {
        result = true;
      }
    }
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result));
  }

 private:
  ~MockHidConnection() override {}
};

class TestDevicePermissionsPrompt
    : public DevicePermissionsPrompt,
      public DevicePermissionsPrompt::Prompt::Observer {
 public:
  explicit TestDevicePermissionsPrompt(content::WebContents* web_contents)
      : DevicePermissionsPrompt(web_contents) {}

  ~TestDevicePermissionsPrompt() override { prompt()->SetObserver(nullptr); }

  void ShowDialog() override { prompt()->SetObserver(this); }

  void OnDeviceAdded(size_t index, const base::string16& device_name) override {
    OnDevicesChanged();
  }

  void OnDeviceRemoved(size_t index,
                       const base::string16& device_name) override {
    OnDevicesChanged();
  }

 private:
  void OnDevicesChanged() {
    if (prompt()->multiple()) {
      for (size_t i = 0; i < prompt()->GetDeviceCount(); ++i) {
        prompt()->GrantDevicePermission(i);
      }
      prompt()->Dismissed();
    } else {
      for (size_t i = 0; i < prompt()->GetDeviceCount(); ++i) {
        // Always choose the device whose serial number is "A".
        if (prompt()->GetDeviceSerialNumber(i) == base::UTF8ToUTF16("A")) {
          prompt()->GrantDevicePermission(i);
          prompt()->Dismissed();
          return;
        }
      }
    }
  }
};

class TestExtensionsAPIClient : public ShellExtensionsAPIClient {
 public:
  TestExtensionsAPIClient() : ShellExtensionsAPIClient() {}

  std::unique_ptr<DevicePermissionsPrompt> CreateDevicePermissionsPrompt(
      content::WebContents* web_contents) const override {
    return std::make_unique<TestDevicePermissionsPrompt>(web_contents);
  }
};

class HidApiTest : public ShellApiTest {
 public:
  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();

    // MockDeviceClient replaces ShellDeviceClient.
    device_client_.reset(new MockDeviceClient());

    ON_CALL(*device_client_->hid_service(), Connect(_, _))
        .WillByDefault(Invoke(this, &HidApiTest::Connect));
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&HidApiTest::LazyFirstEnumeration, base::Unretained(this)));
  }

  void Connect(const std::string& device_guid,
               const device::HidService::ConnectCallback& callback) {
    const auto& devices = device_client_->hid_service()->devices();
    const auto& device_entry = devices.find(device_guid);
    scoped_refptr<MockHidConnection> connection;
    if (device_entry != devices.end()) {
      connection = new MockHidConnection(device_entry->second);
    }

    ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                            base::Bind(callback, connection));
  }

  void LazyFirstEnumeration() {
    AddDevice(kTestDeviceIds[0], 0x18D1, 0x58F0, false, "A");
    AddDevice(kTestDeviceIds[1], 0x18D1, 0x58F0, true, "B");
    AddDevice(kTestDeviceIds[2], 0x18D1, 0x58F1, false, "C");
    device_client_->hid_service()->FirstEnumerationComplete();
  }

  void AddDevice(const HidPlatformDeviceId& platform_device_id,
                 int vendor_id,
                 int product_id,
                 bool report_id,
                 std::string serial_number) {
    std::vector<uint8_t> report_descriptor;
    if (report_id) {
      report_descriptor.insert(
          report_descriptor.begin(), kReportDescriptorWithIDs,
          kReportDescriptorWithIDs + sizeof(kReportDescriptorWithIDs));
    } else {
      report_descriptor.insert(report_descriptor.begin(), kReportDescriptor,
                               kReportDescriptor + sizeof(kReportDescriptor));
    }
    device_client_->hid_service()->AddDevice(new HidDeviceInfo(
        platform_device_id, vendor_id, product_id, "Test Device", serial_number,
        device::mojom::HidBusType::kHIDBusTypeUSB, report_descriptor));
  }

 protected:
  std::unique_ptr<MockDeviceClient> device_client_;
};

IN_PROC_BROWSER_TEST_F(HidApiTest, HidApp) {
  ASSERT_TRUE(RunAppTest("api_test/hid/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(HidApiTest, OnDeviceAdded) {
  ExtensionTestMessageListener load_listener("loaded", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/hid/add_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Add a blocked device first so that the test will fail if a notification is
  // received.
  AddDevice(kTestDeviceIds[3], 0x18D1, 0x58F1, false, "A");
  AddDevice(kTestDeviceIds[4], 0x18D1, 0x58F0, false, "A");
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  EXPECT_EQ("success", result_listener.message());
}

IN_PROC_BROWSER_TEST_F(HidApiTest, OnDeviceRemoved) {
  ExtensionTestMessageListener load_listener("loaded", false);
  ExtensionTestMessageListener result_listener("success", false);
  result_listener.set_failure_message("failure");

  ASSERT_TRUE(LoadApp("api_test/hid/remove_event"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Device C was not returned by chrome.hid.getDevices, the app will not get
  // a notification.
  device_client_->hid_service()->RemoveDevice(kTestDeviceIds[2]);
  // Device A was returned, the app will get a notification.
  device_client_->hid_service()->RemoveDevice(kTestDeviceIds[0]);
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  EXPECT_EQ("success", result_listener.message());
}

IN_PROC_BROWSER_TEST_F(HidApiTest, GetUserSelectedDevices) {
  ExtensionTestMessageListener open_listener("opened_device", false);

  TestExtensionsAPIClient test_api_client;
  ASSERT_TRUE(LoadApp("api_test/hid/get_user_selected_devices"));
  ASSERT_TRUE(open_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener remove_listener("removed", false);
  device_client_->hid_service()->RemoveDevice(kTestDeviceIds[0]);
  ASSERT_TRUE(remove_listener.WaitUntilSatisfied());

  ExtensionTestMessageListener add_listener("added", false);
  AddDevice(kTestDeviceIds[0], 0x18D1, 0x58F0, true, "A");
  ASSERT_TRUE(add_listener.WaitUntilSatisfied());
}

}  // namespace extensions
