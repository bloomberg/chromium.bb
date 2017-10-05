// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "device/hid/hid_device_filter.h"
#include "device/hid/public/interfaces/hid.mojom.h"
#include "device/u2f/fake_hid_impl_for_testing.h"
#include "device/u2f/u2f_apdu_command.h"
#include "device/u2f/u2f_apdu_response.h"
#include "device/u2f/u2f_hid_device.h"
#include "device/u2f/u2f_packet.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void ResponseCallback(std::unique_ptr<device::U2fApduResponse>* output,
                      bool success,
                      std::unique_ptr<device::U2fApduResponse> response) {
  *output = std::move(response);
}

}  // namespace

namespace device {

class U2fDeviceEnumerate {
 public:
  explicit U2fDeviceEnumerate(device::mojom::HidManager* hid_manager)
      : closure_(),
        callback_(base::BindOnce(&U2fDeviceEnumerate::ReceivedCallback,
                                 base::Unretained(this))),
        hid_manager_(hid_manager),
        run_loop_() {}
  ~U2fDeviceEnumerate() {}

  void ReceivedCallback(std::vector<device::mojom::HidDeviceInfoPtr> devices) {
    std::list<std::unique_ptr<U2fHidDevice>> u2f_devices;
    filter_.SetUsagePage(0xf1d0);
    for (auto& device_info : devices) {
      if (filter_.Matches(*device_info))
        u2f_devices.push_front(std::make_unique<U2fHidDevice>(
            std::move(device_info), hid_manager_));
    }
    devices_ = std::move(u2f_devices);
    closure_.Run();
  }

  std::list<std::unique_ptr<U2fHidDevice>>& WaitForCallback() {
    closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
    return devices_;
  }

  device::mojom::HidManager::GetDevicesCallback callback() {
    return std::move(callback_);
  }

 private:
  HidDeviceFilter filter_;
  std::list<std::unique_ptr<U2fHidDevice>> devices_;
  base::Closure closure_;
  device::mojom::HidManager::GetDevicesCallback callback_;
  device::mojom::HidManager* hid_manager_;
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

  void SetUp() override {
    fake_hid_manager_ = std::make_unique<FakeHidManager>();
    fake_hid_manager_->AddBinding2(mojo::MakeRequest(&hid_manager_));
  }

 protected:
  device::mojom::HidManagerPtr hid_manager_;
  std::unique_ptr<FakeHidManager> fake_hid_manager_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

TEST_F(U2fHidDeviceTest, TestHidDeviceVersion) {
  if (!U2fHidDevice::IsTestEnabled())
    return;

  U2fDeviceEnumerate callback(hid_manager_.get());
  hid_manager_->GetDevices(callback.callback());
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

  U2fDeviceEnumerate callback(hid_manager_.get());
  hid_manager_->GetDevices(callback.callback());
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
  U2fDeviceEnumerate callback(hid_manager_.get());

  HidCollectionInfo c_info;
  c_info.usage = HidUsageAndPage(1, 0xf1d0);

  auto hid_device = device::mojom::HidDeviceInfo::New();
  hid_device->guid = "A";
  hid_device->product_name = "Test Fido device";
  hid_device->serial_number = "123FIDO";
  hid_device->bus_type = device::mojom::HidBusType::kHIDBusTypeUSB;
  hid_device->collections.push_back(c_info);
  hid_device->max_input_report_size = 64;
  hid_device->max_output_report_size = 64;

  fake_hid_manager_->AddDevice(std::move(hid_device));
  hid_manager_->GetDevices(callback.callback());

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
  U2fDeviceEnumerate callback(hid_manager_.get());

  HidCollectionInfo c_info;
  c_info.usage = HidUsageAndPage(1, static_cast<HidUsageAndPage::Page>(0xf1d0));

  auto hid_device = device::mojom::HidDeviceInfo::New();
  hid_device->guid = "A";
  hid_device->product_name = "Test Fido device";
  hid_device->serial_number = "123FIDO";
  hid_device->bus_type = device::mojom::HidBusType::kHIDBusTypeUSB;
  hid_device->collections.push_back(c_info);
  hid_device->max_input_report_size = 64;
  hid_device->max_output_report_size = 64;

  fake_hid_manager_->AddDevice(std::move(hid_device));
  hid_manager_->GetDevices(callback.callback());

  std::list<std::unique_ptr<U2fHidDevice>>& u2f_devices =
      callback.WaitForCallback();

  ASSERT_EQ(static_cast<size_t>(1), u2f_devices.size());
  auto& device = u2f_devices.front();
  // Mock connection where writes always fail
  FakeHidConnection::mock_connection_error_ = true;
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
  FakeHidConnection::mock_connection_error_ = false;

  EXPECT_EQ(U2fHidDevice::State::DEVICE_ERROR, device->state_);
  EXPECT_EQ(nullptr, response1);
  EXPECT_EQ(nullptr, response2);
  EXPECT_EQ(nullptr, response3);
};

}  // namespace device
