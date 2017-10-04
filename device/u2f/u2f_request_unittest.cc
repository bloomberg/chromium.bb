// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_io_thread.h"
#include "device/base/mock_device_client.h"
#include "device/hid/mock_hid_service.h"
#include "device/hid/public/interfaces/hid.mojom.h"
#include "device/test/test_device_client.h"
#include "device/test/usb_test_gadget.h"
#include "device/u2f/u2f_hid_device.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "u2f_request.h"

namespace device {
namespace {
#if defined(OS_MACOSX)
const uint64_t kTestDeviceId0 = 42;
const uint64_t kTestDeviceId1 = 43;
const uint64_t kTestDeviceId2 = 44;
#else
const char* kTestDeviceId0 = "device0";
const char* kTestDeviceId1 = "device1";
const char* kTestDeviceId2 = "device2";
#endif

class FakeU2fRequest : public U2fRequest {
 public:
  FakeU2fRequest(const ResponseCallback& cb) : U2fRequest(cb) {}
  ~FakeU2fRequest() override {}

  void TryDevice() override {
    cb_.Run(U2fReturnCode::SUCCESS, std::vector<uint8_t>());
  }
};
}  // namespace

class TestResponseCallback {
 public:
  TestResponseCallback()
      : callback_(base::Bind(&TestResponseCallback::ReceivedCallback,
                             base::Unretained(this))) {}
  ~TestResponseCallback() {}

  void ReceivedCallback(U2fReturnCode status,
                        const std::vector<uint8_t>& data) {
    closure_.Run();
  }

  void WaitForCallback() {
    closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
  }

  U2fRequest::ResponseCallback& callback() { return callback_; }

 private:
  base::Closure closure_;
  U2fRequest::ResponseCallback callback_;
  base::RunLoop run_loop_;
};

class U2fRequestTest : public testing::Test {
 public:
  U2fRequestTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        io_thread_(base::TestIOThread::kAutoStart) {}

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::TestIOThread io_thread_;
  device::MockDeviceClient device_client_;
};

TEST_F(U2fRequestTest, TestAddRemoveDevice) {
  MockHidService* hid_service = device_client_.hid_service();
  HidCollectionInfo c_info;
  hid_service->FirstEnumerationComplete();

  TestResponseCallback cb;
  FakeU2fRequest request(cb.callback());
  request.Enumerate();

  c_info.usage = HidUsageAndPage(1, static_cast<HidUsageAndPage::Page>(0xf1d0));
  scoped_refptr<HidDeviceInfo> u2f_device_0 =
      base::WrapRefCounted(new HidDeviceInfo(
          kTestDeviceId0, 0, 0, "Test Fido Device", "123FIDO",
          device::mojom::HidBusType::kHIDBusTypeUSB, c_info, 64, 64, 0));
  hid_service->AddDevice(u2f_device_0);

  // Make sure the enumeration is finshed, so HidService is ready to send
  // the OnDeviceAdded or OnDeviceRemoved to its observers.
  cb.WaitForCallback();
  EXPECT_EQ(static_cast<size_t>(0), request.devices_.size());

  // Add one U2F device
  scoped_refptr<HidDeviceInfo> u2f_device_1 =
      base::WrapRefCounted(new HidDeviceInfo(
          kTestDeviceId1, 0, 0, "Test Fido Device", "123FIDO",
          device::mojom::HidBusType::kHIDBusTypeUSB, c_info, 64, 64, 0));
  hid_service->AddDevice(u2f_device_1);
  EXPECT_EQ(static_cast<size_t>(1), request.devices_.size());

  // Add one non-U2F device. Verify that it is not added to our device list.
  scoped_refptr<HidDeviceInfo> other_device =
      base::WrapRefCounted(new HidDeviceInfo(
          kTestDeviceId2, 0, 0, "Other Device", "OtherDevice",
          device::mojom::HidBusType::kHIDBusTypeUSB, std::vector<uint8_t>()));
  hid_service->AddDevice(other_device);
  EXPECT_EQ(static_cast<size_t>(1), request.devices_.size());

  // Remove the non-U2F device and verify that device list was unchanged.
  hid_service->RemoveDevice(kTestDeviceId2);
  EXPECT_EQ(static_cast<size_t>(1), request.devices_.size());

  // Remove the U2F device and verify that device list is empty.
  hid_service->RemoveDevice(kTestDeviceId1);
  EXPECT_EQ(static_cast<size_t>(0), request.devices_.size());
}

TEST_F(U2fRequestTest, TestIterateDevice) {
  TestResponseCallback cb;
  FakeU2fRequest request(cb.callback());
  HidCollectionInfo c_info;
  // Add one U2F device and one non-U2f device
  c_info.usage = HidUsageAndPage(1, static_cast<HidUsageAndPage::Page>(0xf1d0));
  scoped_refptr<HidDeviceInfo> device0 = base::WrapRefCounted(new HidDeviceInfo(
      kTestDeviceId0, 0, 0, "Test Fido Device", "123FIDO",
      device::mojom::HidBusType::kHIDBusTypeUSB, c_info, 64, 64, 0));
  request.devices_.push_back(
      std::make_unique<U2fHidDevice>(device0->device()->Clone()));
  scoped_refptr<HidDeviceInfo> device1 = base::WrapRefCounted(new HidDeviceInfo(
      kTestDeviceId1, 0, 0, "Test Fido Device", "123FIDO",
      device::mojom::HidBusType::kHIDBusTypeUSB, c_info, 64, 64, 0));
  request.devices_.push_back(
      std::make_unique<U2fHidDevice>(device1->device()->Clone()));

  // Move first device to current
  request.IterateDevice();
  ASSERT_NE(nullptr, request.current_device_);
  EXPECT_EQ(static_cast<size_t>(1), request.devices_.size());

  // Move second device to current, first to attempted
  request.IterateDevice();
  ASSERT_NE(nullptr, request.current_device_);
  EXPECT_EQ(static_cast<size_t>(1), request.attempted_devices_.size());

  // Move second device from current to attempted, move attempted to devices as
  // all devices have been attempted
  request.IterateDevice();
  ASSERT_EQ(nullptr, request.current_device_);
  EXPECT_EQ(static_cast<size_t>(2), request.devices_.size());
  EXPECT_EQ(static_cast<size_t>(0), request.attempted_devices_.size());
}

TEST_F(U2fRequestTest, TestBasicMachine) {
  MockHidService* hid_service = device_client_.hid_service();
  hid_service->FirstEnumerationComplete();
  TestResponseCallback cb;
  FakeU2fRequest request(cb.callback());
  request.Start();
  // Add one U2F device
  HidCollectionInfo c_info;
  c_info.usage = HidUsageAndPage(1, static_cast<HidUsageAndPage::Page>(0xf1d0));
  scoped_refptr<HidDeviceInfo> u2f_device =
      base::WrapRefCounted(new HidDeviceInfo(
          kTestDeviceId0, 0, 0, "Test Fido Device", "123FIDO",
          device::mojom::HidBusType::kHIDBusTypeUSB, c_info, 64, 64, 0));
  hid_service->AddDevice(u2f_device);
  cb.WaitForCallback();
  EXPECT_EQ(U2fRequest::State::BUSY, request.state_);
}

}  // namespace device
