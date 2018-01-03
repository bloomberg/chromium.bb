// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "device/u2f/fake_hid_impl_for_testing.h"
#include "device/u2f/u2f_apdu_command.h"
#include "device/u2f/u2f_apdu_response.h"
#include "device/u2f/u2f_command_type.h"
#include "device/u2f/u2f_hid_device.h"
#include "device/u2f/u2f_message.h"
#include "device/u2f/u2f_packet.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/device/public/cpp/hid/hid_device_filter.h"
#include "services/device/public/interfaces/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArg;
using ::testing::WithArgs;

namespace {

void ResponseCallback(std::unique_ptr<device::U2fApduResponse>* output,
                      bool success,
                      std::unique_ptr<device::U2fApduResponse> response) {
  *output = std::move(response);
}

std::string HexEncode(base::span<const uint8_t> in) {
  return base::HexEncode(in.data(), in.size());
}

std::vector<uint8_t> HexDecode(base::StringPiece in) {
  std::vector<uint8_t> out;
  bool result = base::HexStringToBytes(in.as_string(), &out);
  DCHECK(result);
  return out;
}

// Converts hex encoded StringPiece to byte vector and pads zero to fit HID
// packet size.
std::vector<uint8_t> MakePacket(base::StringPiece hex) {
  std::vector<uint8_t> out = HexDecode(hex);
  out.resize(64);
  return out;
}

// Returns HID_INIT request to send to device with mock connection.
std::vector<uint8_t> CreateMockHidInitResponse(
    std::vector<uint8_t> nonce,
    std::vector<uint8_t> channel_id) {
  // 4 bytes of broadcast channel identifier(ffffffff), followed by
  // HID_INIT command(86) and 2 byte payload length(11)
  return MakePacket("ffffffff860011" + HexEncode(nonce) +
                    HexEncode(channel_id));
}

// Returns "U2F_v2" as a mock response to version request with given channel id.
std::vector<uint8_t> CreateMockVersionResponse(
    std::vector<uint8_t> channel_id) {
  // HID_MSG command(83), followed by payload length(0008), followed by
  // hex encoded "U2F_V2" and  NO_ERROR response code(9000).
  return MakePacket(HexEncode(channel_id) + "8300085532465f56329000");
}

device::mojom::HidDeviceInfoPtr TestHidDevice() {
  auto c_info = device::mojom::HidCollectionInfo::New();
  c_info->usage = device::mojom::HidUsageAndPage::New(1, 0xf1d0);
  auto hid_device = device::mojom::HidDeviceInfo::New();
  hid_device->guid = "A";
  hid_device->product_name = "Test Fido device";
  hid_device->serial_number = "123FIDO";
  hid_device->bus_type = device::mojom::HidBusType::kHIDBusTypeUSB;
  hid_device->collections.push_back(std::move(c_info));
  hid_device->max_input_report_size = 64;
  hid_device->max_output_report_size = 64;
  return hid_device;
}

}  // namespace

class U2fDeviceEnumerate {
 public:
  explicit U2fDeviceEnumerate(device::mojom::HidManager* hid_manager)
      : closure_(),
        callback_(base::BindOnce(&U2fDeviceEnumerate::ReceivedCallback,
                                 base::Unretained(this))),
        hid_manager_(hid_manager),
        run_loop_() {}
  ~U2fDeviceEnumerate() = default;

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
        callback_(base::BindOnce(&TestVersionCallback::ReceivedCallback,
                                 base::Unretained(this))),
        run_loop_() {}
  ~TestVersionCallback() = default;

  void ReceivedCallback(bool success, U2fDevice::ProtocolVersion version) {
    version_ = version;
    std::move(closure_).Run();
  }

  U2fDevice::ProtocolVersion WaitForCallback() {
    closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
    return version_;
  }

  U2fDevice::VersionCallback callback() { return std::move(callback_); }

 private:
  U2fDevice::ProtocolVersion version_;
  base::OnceClosure closure_;
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
  ~TestDeviceCallback() = default;

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
}

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
}

TEST_F(U2fHidDeviceTest, TestConnectionFailure) {
  // Setup and enumerate mock device
  U2fDeviceEnumerate callback(hid_manager_.get());
  auto hid_device = TestHidDevice();
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
  device->pending_transactions_.emplace(
      U2fApduCommand::CreateVersion(),
      base::BindOnce(&ResponseCallback, &response1));
  std::unique_ptr<U2fApduResponse> response2(
      U2fApduResponse::CreateFromMessage(std::vector<uint8_t>({0x0, 0x0})));
  device->pending_transactions_.emplace(
      U2fApduCommand::CreateVersion(),
      base::BindOnce(&ResponseCallback, &response2));
  std::unique_ptr<U2fApduResponse> response3(
      U2fApduResponse::CreateFromMessage(std::vector<uint8_t>({0x0, 0x0})));
  device->DeviceTransact(U2fApduCommand::CreateVersion(),
                         base::Bind(&ResponseCallback, &response3));
  EXPECT_EQ(U2fHidDevice::State::DEVICE_ERROR, device->state_);
  EXPECT_EQ(nullptr, response1);
  EXPECT_EQ(nullptr, response2);
  EXPECT_EQ(nullptr, response3);
}

TEST_F(U2fHidDeviceTest, TestDeviceError) {
  // Setup and enumerate mock device
  U2fDeviceEnumerate callback(hid_manager_.get());

  auto hid_device = TestHidDevice();

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
  device->pending_transactions_.emplace(
      U2fApduCommand::CreateVersion(),
      base::BindOnce(&ResponseCallback, &response1));
  std::unique_ptr<U2fApduResponse> response2(
      U2fApduResponse::CreateFromMessage(std::vector<uint8_t>({0x0, 0x0})));
  device->pending_transactions_.emplace(
      U2fApduCommand::CreateVersion(),
      base::BindOnce(&ResponseCallback, &response2));
  std::unique_ptr<U2fApduResponse> response3(
      U2fApduResponse::CreateFromMessage(std::vector<uint8_t>({0x0, 0x0})));
  device->DeviceTransact(U2fApduCommand::CreateVersion(),
                         base::Bind(&ResponseCallback, &response3));
  FakeHidConnection::mock_connection_error_ = false;

  EXPECT_EQ(U2fHidDevice::State::DEVICE_ERROR, device->state_);
  EXPECT_EQ(nullptr, response1);
  EXPECT_EQ(nullptr, response2);
  EXPECT_EQ(nullptr, response3);
}

TEST_F(U2fHidDeviceTest, TestRetryChannelAllocation) {
  const std::vector<uint8_t> kIncorrectNonce = {0x00, 0x00, 0x00, 0x00,
                                                0x00, 0x00, 0x00, 0x00};

  const std::vector<uint8_t> kChannelId = {0x01, 0x02, 0x03, 0x04};

  U2fDeviceEnumerate callback(hid_manager_.get());
  auto hid_device = TestHidDevice();

  // Replace device HID connection with custom client connection bound to mock
  // server-side mojo connection.
  device::mojom::HidConnectionPtr connection_client;
  MockHidConnection mock_connection(
      hid_device.Clone(), mojo::MakeRequest(&connection_client), kChannelId);

  // Delegate custom functions to be invoked for mock hid connection
  EXPECT_CALL(mock_connection, WritePtr(_, _, _))
      // HID_INIT request to authenticator for channel allocation.
      .WillOnce(WithArgs<1, 2>(
          Invoke([&](const std::vector<uint8_t>& buffer,
                     device::mojom::HidConnection::WriteCallback* cb) {
            mock_connection.SetNonce(base::make_span(buffer).subspan(7, 8));
            std::move(*cb).Run(true);
          })))

      // HID_MSG request to authenticator for version request.
      .WillOnce(WithArgs<2>(
          Invoke([](device::mojom::HidConnection::WriteCallback* cb) {
            std::move(*cb).Run(true);
          })));

  EXPECT_CALL(mock_connection, ReadPtr(_))
      // First response to HID_INIT request with incorrect nonce.
      .WillOnce(WithArg<0>(
          Invoke([kIncorrectNonce, &mock_connection](
                     device::mojom::HidConnection::ReadCallback* cb) {
            std::move(*cb).Run(
                true, 0,
                CreateMockHidInitResponse(
                    kIncorrectNonce, mock_connection.connection_channel_id()));
          })))
      // Second response to HID_INIT request with correct nonce.
      .WillOnce(WithArg<0>(Invoke(
          [&mock_connection](device::mojom::HidConnection::ReadCallback* cb) {
            std::move(*cb).Run(true, 0,
                               CreateMockHidInitResponse(
                                   mock_connection.nonce(),
                                   mock_connection.connection_channel_id()));
          })))
      // Version response from the authenticator.
      .WillOnce(WithArg<0>(Invoke(
          [&mock_connection](device::mojom::HidConnection::ReadCallback* cb) {
            std::move(*cb).Run(true, 0,
                               CreateMockVersionResponse(
                                   mock_connection.connection_channel_id()));
          })));

  // Add device and set mock connection to fake hid manager
  fake_hid_manager_->AddDeviceAndSetConnection(std::move(hid_device),
                                               std::move(connection_client));

  hid_manager_->GetDevices(callback.callback());
  std::list<std::unique_ptr<U2fHidDevice>>& u2f_devices =
      callback.WaitForCallback();

  ASSERT_EQ(1u, u2f_devices.size());
  auto& device = u2f_devices.front();
  TestVersionCallback vc;
  device->Version(vc.callback());
  EXPECT_EQ(vc.WaitForCallback(), U2fDevice::ProtocolVersion::U2F_V2);
}

}  // namespace device
