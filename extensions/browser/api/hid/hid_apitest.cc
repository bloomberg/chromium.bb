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
#include "net/base/io_buffer.h"

namespace extensions {

namespace {

using base::ThreadTaskRunnerHandle;
using device::HidCollectionInfo;
using device::HidConnection;
using device::HidDeviceId;
using device::HidDeviceInfo;
using device::HidService;
using device::HidUsageAndPage;
using net::IOBuffer;

class MockHidConnection : public HidConnection {
 public:
  MockHidConnection(const HidDeviceInfo& device_info)
      : HidConnection(device_info) {}

  void PlatformClose() override {}

  void PlatformRead(const ReadCallback& callback) override {
    const char kResult[] = "This is a HID input report.";
    uint8_t report_id = device_info().has_report_id ? 1 : 0;
    scoped_refptr<IOBuffer> buffer(new IOBuffer(sizeof(kResult)));
    buffer->data()[0] = report_id;
    memcpy(buffer->data() + 1, kResult, sizeof(kResult) - 1);
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, true, buffer, sizeof(kResult)));
  }

  void PlatformWrite(scoped_refptr<net::IOBuffer> buffer,
                     size_t size,
                     const WriteCallback& callback) override {
    const char kExpected[] = "This is a HID output report.";
    bool result = false;
    if (size == sizeof(kExpected)) {
      uint8_t report_id = buffer->data()[0];
      uint8_t expected_report_id = device_info().has_report_id ? 1 : 0;
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
    if (device_info().has_report_id) {
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
      uint8_t expected_report_id = device_info().has_report_id ? 1 : 0;
      if (report_id == expected_report_id &&
          memcmp(buffer->data() + 1, kExpected, sizeof(kExpected) - 1) == 0) {
        result = true;
      }
    }
    ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                            base::Bind(callback, result));
  }

 private:
  ~MockHidConnection() {}
};

class MockHidService : public HidService {
 public:
  MockHidService() : HidService() {
    {
      HidDeviceInfo device_info;
      device_info.device_id = "Device A";
      device_info.vendor_id = 0x18D1;
      device_info.product_id = 0x58F0;
      device_info.max_input_report_size = 128;
      device_info.max_output_report_size = 128;
      device_info.max_feature_report_size = 128;
      {
        HidCollectionInfo collection_info;
        device_info.collections.push_back(collection_info);
      }
      AddDevice(device_info);
    }

    {
      HidDeviceInfo device_info;
      device_info.device_id = "Device B";
      device_info.vendor_id = 0x18D1;
      device_info.product_id = 0x58F0;
      device_info.max_input_report_size = 128;
      device_info.max_output_report_size = 128;
      device_info.max_feature_report_size = 128;
      {
        HidCollectionInfo collection_info;
        collection_info.usage =
            HidUsageAndPage(0, HidUsageAndPage::kPageVendor);
        collection_info.report_ids.insert(1);
        device_info.has_report_id = true;
        device_info.collections.push_back(collection_info);
      }
      AddDevice(device_info);
    }

    {
      HidDeviceInfo device_info;
      device_info.device_id = "Device C";
      device_info.vendor_id = 0x18D1;
      device_info.product_id = 0x58F1;
      device_info.max_input_report_size = 128;
      device_info.max_output_report_size = 128;
      device_info.max_feature_report_size = 128;
      {
        HidCollectionInfo collection_info;
        device_info.collections.push_back(collection_info);
      }
      AddDevice(device_info);
    }
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
};

}  // namespace

class HidApiTest : public ShellApiTest {
 public:
  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();
    HidService::SetInstanceForTest(new MockHidService());
  }
};

IN_PROC_BROWSER_TEST_F(HidApiTest, HidApp) {
  ASSERT_TRUE(RunAppTest("api_test/hid/api")) << message_;
}

}  // namespace extensions
