// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "device/hid/hid_collection_info.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/hid_service.h"
#include "device/hid/hid_usage_and_page.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/base/io_buffer.h"

using base::ThreadTaskRunnerHandle;
using device::HidCollectionInfo;
using device::HidConnection;
using device::HidDeviceId;
using device::HidDeviceInfo;
using device::HidService;
using device::HidUsageAndPage;
using net::IOBuffer;

namespace device {

// These report descriptors define two devices with 8-byte input, output and
// feature reports. The first implements usage page 0xFF00 and has a single
// report without and ID. The second implements usage page 0xFF01 and has a
// single report with ID 1.
const uint8 kReportDescriptor[] = {0x06, 0x00, 0xFF, 0x08, 0xA1, 0x01, 0x15,
                                   0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95,
                                   0x08, 0x08, 0x81, 0x02, 0x08, 0x91, 0x02,
                                   0x08, 0xB1, 0x02, 0xC0};
const uint8 kReportDescriptorWithIDs[] = {
    0x06, 0x01, 0xFF, 0x08, 0xA1, 0x01, 0x15, 0x00, 0x26,
    0xFF, 0x00, 0x85, 0x01, 0x75, 0x08, 0x95, 0x08, 0x08,
    0x81, 0x02, 0x08, 0x91, 0x02, 0x08, 0xB1, 0x02, 0xC0};

class MockHidConnection : public HidConnection {
 public:
  MockHidConnection(scoped_refptr<HidDeviceInfo> device_info)
      : HidConnection(device_info) {}

  void PlatformClose() override {}

  void PlatformRead(const ReadCallback& callback) override {
    const char kResult[] = "This is a HID input report.";
    uint8_t report_id = device_info()->has_report_id() ? 1 : 0;
    scoped_refptr<IOBuffer> buffer(new IOBuffer(sizeof(kResult)));
    buffer->data()[0] = report_id;
    memcpy(buffer->data() + 1, kResult, sizeof(kResult) - 1);
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, true, buffer, sizeof(kResult)));
  }

  void PlatformWrite(scoped_refptr<net::IOBuffer> buffer,
                     size_t size,
                     const WriteCallback& callback) override {
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
    ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                            base::Bind(callback, result));
  }

  void PlatformGetFeatureReport(uint8_t report_id,
                                const ReadCallback& callback) override {
    const char kResult[] = "This is a HID feature report.";
    scoped_refptr<IOBuffer> buffer(new IOBuffer(sizeof(kResult)));
    size_t offset = 0;
    if (device_info()->has_report_id()) {
      buffer->data()[offset++] = report_id;
    }
    memcpy(buffer->data() + offset, kResult, sizeof(kResult) - 1);
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, true, buffer, sizeof(kResult) - 1 + offset));
  }

  void PlatformSendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                 size_t size,
                                 const WriteCallback& callback) override {
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
    ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                            base::Bind(callback, result));
  }

 private:
  ~MockHidConnection() override {}
};

class MockHidService : public HidService {
 public:
  MockHidService() : HidService() {
    // Verify that devices are enumerated properly even when the first
    // enumeration happens asynchronously.
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&MockHidService::LazyFirstEnumeration,
                              base::Unretained(this)));
  }

  void Connect(const HidDeviceId& device_id,
               const ConnectCallback& callback) override {
    const auto& device_entry = devices().find(device_id);
    scoped_refptr<HidConnection> connection;
    if (device_entry != devices().end()) {
      connection = new MockHidConnection(device_entry->second);
    }

    ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                            base::Bind(callback, connection));
  }

  void LazyFirstEnumeration() {
    AddDevice("A", 0x18D1, 0x58F0, false);
    AddDevice("B", 0x18D1, 0x58F0, true);
    AddDevice("C", 0x18D1, 0x58F1, false);
    FirstEnumerationComplete();
  }

  void AddDevice(const std::string& device_id,
                 int vendor_id,
                 int product_id,
                 bool report_id) {
    std::vector<uint8> report_descriptor;
    if (report_id) {
      report_descriptor.insert(
          report_descriptor.begin(), kReportDescriptorWithIDs,
          kReportDescriptorWithIDs + sizeof(kReportDescriptorWithIDs));
    } else {
      report_descriptor.insert(report_descriptor.begin(), kReportDescriptor,
                               kReportDescriptor + sizeof(kReportDescriptor));
    }
    HidService::AddDevice(new HidDeviceInfo(device_id, vendor_id, product_id,
                                            "", "", kHIDBusTypeUSB,
                                            report_descriptor));
  }

  void RemoveDevice(const std::string& device_id) {
    HidService::RemoveDevice(device_id);
  }
};

}  // namespace device

namespace extensions {

class HidApiTest : public ShellApiTest {
 public:
  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();
    hid_service_ = new device::MockHidService();
    HidService::SetInstanceForTest(hid_service_);
  }

 protected:
  device::MockHidService* hid_service_;
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
  hid_service_->AddDevice("D", 0x18D1, 0x58F1, false);
  hid_service_->AddDevice("E", 0x18D1, 0x58F0, false);
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
  hid_service_->RemoveDevice("C");
  // Device A was returned, the app will get a notification.
  hid_service_->RemoveDevice("A");
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
  EXPECT_EQ("success", result_listener.message());
}

}  // namespace extensions
