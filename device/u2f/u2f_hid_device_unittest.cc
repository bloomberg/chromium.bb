// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "device/base/mock_device_client.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_device_filter.h"
#include "device/hid/mock_hid_service.h"
#include "device/test/test_device_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "u2f_apdu_command.h"
#include "u2f_apdu_response.h"
#include "u2f_hid_device.h"
#include "u2f_packet.h"

namespace {

#if defined(OS_MACOSX)
const uint64_t kTestDeviceId = 42;
#else
const char* kTestDeviceId = "device";
#endif

void ResponseCallback(std::unique_ptr<device::U2fApduResponse>* output,
                      bool success,
                      std::unique_ptr<device::U2fApduResponse> response) {
  *output = std::move(response);
}

class MockHidErrorConnection : public device::HidConnection {
 public:
  explicit MockHidErrorConnection(
      scoped_refptr<device::HidDeviceInfo> device_info)
      : device::HidConnection(device_info) {}

  void PlatformClose() override {}

  void PlatformRead(ReadCallback callback) override {}

  void PlatformWrite(scoped_refptr<net::IOBuffer> buffer,
                     size_t size,
                     WriteCallback callback) override {
    std::move(callback).Run(false);
  }

  void PlatformGetFeatureReport(uint8_t report_id,
                                ReadCallback callback) override {}

  void PlatformSendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                 size_t size,
                                 WriteCallback callback) override {}

 private:
  ~MockHidErrorConnection() override {}
};

}  // namespace

namespace device {

class U2fDeviceEnumerate {
 public:
  U2fDeviceEnumerate()
      : closure_(),
        callback_(base::Bind(&U2fDeviceEnumerate::ReceivedCallback,
                             base::Unretained(this))),
        run_loop_() {}
  ~U2fDeviceEnumerate() {}

  void ReceivedCallback(
      const std::vector<scoped_refptr<HidDeviceInfo>>& devices) {
    std::list<std::unique_ptr<U2fHidDevice>> u2f_devices;
    filter_.SetUsagePage(0xf1d0);
    for (auto device_info : devices) {
      if (filter_.Matches(device_info))
        u2f_devices.push_front(base::MakeUnique<U2fHidDevice>(device_info));
    }
    devices_ = std::move(u2f_devices);
    closure_.Run();
  }

  std::list<std::unique_ptr<U2fHidDevice>>& WaitForCallback() {
    closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
    return devices_;
  }

  const HidService::GetDevicesCallback& callback() { return callback_; }

 private:
  HidDeviceFilter filter_;
  std::list<std::unique_ptr<U2fHidDevice>> devices_;
  base::Closure closure_;
  HidService::GetDevicesCallback callback_;
  base::RunLoop run_loop_;
};

class TestVersionCallback {
 public:
  TestVersionCallback()
      : closure_(),
        callback_(base::Bind(&TestVersionCallback::ReceivedCallback,
                             base::Unretained(this))),
        run_loop_() {}
  ~TestVersionCallback() {}

  void ReceivedCallback(bool success, U2fDevice::ProtocolVersion version) {
    version_ = version;
    closure_.Run();
  }

  U2fDevice::ProtocolVersion WaitForCallback() {
    closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
    return version_;
  }

  const U2fDevice::VersionCallback& callback() { return callback_; }

 private:
  U2fDevice::ProtocolVersion version_;
  base::Closure closure_;
  U2fDevice::VersionCallback callback_;
  base::RunLoop run_loop_;
};

class TestDeviceCallback {
 public:
  TestDeviceCallback()
      : closure_(),
        callback_(base::Bind(&TestDeviceCallback::ReceivedCallback,
                             base::Unretained(this))),
        run_loop_() {}
  ~TestDeviceCallback() {}

  void ReceivedCallback(bool success,
                        std::unique_ptr<U2fApduResponse> response) {
    response_ = std::move(response);
    closure_.Run();
  }

  std::unique_ptr<U2fApduResponse> WaitForCallback() {
    closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
    return std::move(response_);
  }

  const U2fDevice::DeviceCallback& callback() { return callback_; }

 private:
  std::unique_ptr<U2fApduResponse> response_;
  base::Closure closure_;
  U2fDevice::DeviceCallback callback_;
  base::RunLoop run_loop_;
};

class U2fHidDeviceTest : public testing::Test {
 public:
  U2fHidDeviceTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestDeviceClient device_client_;
};

TEST_F(U2fHidDeviceTest, TestHidDeviceVersion) {
  if (!U2fHidDevice::IsTestEnabled())
    return;

  U2fDeviceEnumerate callback;
  HidService* hid_service = DeviceClient::Get()->GetHidService();
  hid_service->GetDevices(callback.callback());
  std::list<std::unique_ptr<U2fHidDevice>>& u2f_devices =
      callback.WaitForCallback();

  for (auto& device : u2f_devices) {
    TestVersionCallback vc;
    device->Version(vc.callback());
    U2fDevice::ProtocolVersion version = vc.WaitForCallback();
    EXPECT_EQ(version, U2fDevice::ProtocolVersion::U2F_V2);
  }
};

TEST_F(U2fHidDeviceTest, TestMultipleRequests) {
  if (!U2fHidDevice::IsTestEnabled())
    return;

  U2fDeviceEnumerate callback;
  HidService* hid_service = DeviceClient::Get()->GetHidService();
  hid_service->GetDevices(callback.callback());
  std::list<std::unique_ptr<U2fHidDevice>>& u2f_devices =
      callback.WaitForCallback();

  for (auto& device : u2f_devices) {
    TestVersionCallback vc;
    TestVersionCallback vc2;
    // Call version twice to check message queueing
    device->Version(vc.callback());
    device->Version(vc2.callback());
    U2fDevice::ProtocolVersion version = vc.WaitForCallback();
    EXPECT_EQ(version, U2fDevice::ProtocolVersion::U2F_V2);
    version = vc2.WaitForCallback();
    EXPECT_EQ(version, U2fDevice::ProtocolVersion::U2F_V2);
  }
};

TEST_F(U2fHidDeviceTest, TestConnectionFailure) {
  // Setup and enumerate mock device
  auto client = base::MakeUnique<MockDeviceClient>();
  U2fDeviceEnumerate callback;
  MockHidService* hid_service = client->hid_service();
  HidCollectionInfo c_info;
  c_info.usage = HidUsageAndPage(1, static_cast<HidUsageAndPage::Page>(0xf1d0));
  scoped_refptr<HidDeviceInfo> device0 =
      new HidDeviceInfo(kTestDeviceId, 0, 0, "Test Fido Device", "123FIDO",
                        kHIDBusTypeUSB, c_info, 64, 64, 0);
  hid_service->AddDevice(device0);
  hid_service->FirstEnumerationComplete();
  hid_service->GetDevices(callback.callback());
  std::list<std::unique_ptr<U2fHidDevice>>& u2f_devices =
      callback.WaitForCallback();

  ASSERT_EQ(static_cast<size_t>(1), u2f_devices.size());
  auto& device = u2f_devices.front();
  // Put device in IDLE state
  TestDeviceCallback cb0;
  device->state_ = U2fHidDevice::State::IDLE;

  // Manually delete connection
  device->connection_ = nullptr;
  // Add pending transactions manually and ensure they are processed
  std::unique_ptr<U2fApduResponse> response1(
      U2fApduResponse::CreateFromMessage(std::vector<uint8_t>({0x0, 0x0})));
  device->pending_transactions_.push_back(
      {U2fApduCommand::CreateVersion(),
       base::Bind(&ResponseCallback, &response1)});
  std::unique_ptr<U2fApduResponse> response2(
      U2fApduResponse::CreateFromMessage(std::vector<uint8_t>({0x0, 0x0})));
  device->pending_transactions_.push_back(
      {U2fApduCommand::CreateVersion(),
       base::Bind(&ResponseCallback, &response2)});
  std::unique_ptr<U2fApduResponse> response3(
      U2fApduResponse::CreateFromMessage(std::vector<uint8_t>({0x0, 0x0})));
  device->DeviceTransact(U2fApduCommand::CreateVersion(),
                         base::Bind(&ResponseCallback, &response3));
  EXPECT_EQ(U2fHidDevice::State::DEVICE_ERROR, device->state_);
  EXPECT_EQ(nullptr, response1);
  EXPECT_EQ(nullptr, response2);
  EXPECT_EQ(nullptr, response3);
};

TEST_F(U2fHidDeviceTest, TestDeviceError) {
  // Setup and enumerate mock device
  auto client = base::MakeUnique<MockDeviceClient>();
  U2fDeviceEnumerate callback;
  MockHidService* hid_service = client->hid_service();
  HidCollectionInfo c_info;
  c_info.usage = HidUsageAndPage(1, static_cast<HidUsageAndPage::Page>(0xf1d0));
  scoped_refptr<HidDeviceInfo> device0 =
      new HidDeviceInfo(kTestDeviceId, 0, 0, "Test Fido Device", "123FIDO",
                        kHIDBusTypeUSB, c_info, 64, 64, 0);
  hid_service->AddDevice(device0);
  hid_service->FirstEnumerationComplete();
  hid_service->GetDevices(callback.callback());
  std::list<std::unique_ptr<U2fHidDevice>>& u2f_devices =
      callback.WaitForCallback();

  ASSERT_EQ(static_cast<size_t>(1), u2f_devices.size());
  auto& device = u2f_devices.front();
  // Mock connection where writes always fail
  scoped_refptr<MockHidErrorConnection> connection(
      new MockHidErrorConnection(device0));
  device->connection_ = connection;
  device->state_ = U2fHidDevice::State::IDLE;
  std::unique_ptr<U2fApduResponse> response0(
      U2fApduResponse::CreateFromMessage(std::vector<uint8_t>({0x0, 0x0})));
  device->DeviceTransact(U2fApduCommand::CreateVersion(),
                         base::Bind(&ResponseCallback, &response0));
  EXPECT_EQ(nullptr, response0);
  EXPECT_EQ(U2fHidDevice::State::DEVICE_ERROR, device->state_);

  // Add pending transactions manually and ensure they are processed
  std::unique_ptr<U2fApduResponse> response1(
      U2fApduResponse::CreateFromMessage(std::vector<uint8_t>({0x0, 0x0})));
  device->pending_transactions_.push_back(
      {U2fApduCommand::CreateVersion(),
       base::Bind(&ResponseCallback, &response1)});
  std::unique_ptr<U2fApduResponse> response2(
      U2fApduResponse::CreateFromMessage(std::vector<uint8_t>({0x0, 0x0})));
  device->pending_transactions_.push_back(
      {U2fApduCommand::CreateVersion(),
       base::Bind(&ResponseCallback, &response2)});
  std::unique_ptr<U2fApduResponse> response3(
      U2fApduResponse::CreateFromMessage(std::vector<uint8_t>({0x0, 0x0})));
  device->DeviceTransact(U2fApduCommand::CreateVersion(),
                         base::Bind(&ResponseCallback, &response3));
  EXPECT_EQ(U2fHidDevice::State::DEVICE_ERROR, device->state_);
  EXPECT_EQ(nullptr, response1);
  EXPECT_EQ(nullptr, response2);
  EXPECT_EQ(nullptr, response3);
};

}  // namespace device
