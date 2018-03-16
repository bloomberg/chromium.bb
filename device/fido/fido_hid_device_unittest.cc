// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/apdu/apdu_command.h"
#include "components/apdu/apdu_response.h"
#include "device/fido/fake_hid_impl_for_testing.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_hid_device.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/u2f_request.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/device/public/cpp/hid/hid_device_filter.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArg;
using ::testing::WithArgs;

namespace {

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
std::vector<uint8_t> CreateMockInitResponse(std::vector<uint8_t> nonce,
                                            std::vector<uint8_t> channel_id) {
  // 4 bytes of broadcast channel identifier(ffffffff), followed by
  // HID_INIT command(86) and 2 byte payload length(11).
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

// Returns U2F_V2 version response formatted in APDU response encoding.
std::vector<uint8_t> GetValidU2fVersionResponse() {
  return apdu::ApduResponse(std::vector<uint8_t>(kU2fVersionResponse.begin(),
                                                 kU2fVersionResponse.end()),
                            apdu::ApduResponse::Status::SW_NO_ERROR)
      .GetEncodedResponse();
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

class FidoDeviceEnumerateCallbackReceiver
    : public test::TestCallbackReceiver<std::vector<mojom::HidDeviceInfoPtr>> {
 public:
  explicit FidoDeviceEnumerateCallbackReceiver(
      device::mojom::HidManager* hid_manager)
      : hid_manager_(hid_manager) {}
  ~FidoDeviceEnumerateCallbackReceiver() = default;

  std::vector<std::unique_ptr<FidoHidDevice>> TakeReturnedDevicesFiltered() {
    std::vector<std::unique_ptr<FidoHidDevice>> filtered_results;
    std::vector<mojom::HidDeviceInfoPtr> results;
    std::tie(results) = TakeResult();
    for (auto& device_info : results) {
      HidDeviceFilter filter;
      filter.SetUsagePage(0xf1d0);
      if (filter.Matches(*device_info)) {
        filtered_results.push_back(std::make_unique<FidoHidDevice>(
            std::move(device_info), hid_manager_));
      }
    }
    return filtered_results;
  }

 private:
  device::mojom::HidManager* hid_manager_;

  DISALLOW_COPY_AND_ASSIGN(FidoDeviceEnumerateCallbackReceiver);
};

using TestDeviceCallbackReceiver =
    ::device::test::TestCallbackReceiver<base::Optional<std::vector<uint8_t>>>;

}  // namespace

class FidoHidDeviceTest : public ::testing::Test {
 public:
  FidoHidDeviceTest()
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

TEST_F(FidoHidDeviceTest, TestConnectionFailure) {
  // Setup and enumerate mock device.
  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  auto hid_device = TestHidDevice();
  fake_hid_manager_->AddDevice(std::move(hid_device));
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();

  ASSERT_EQ(static_cast<size_t>(1), u2f_devices.size());
  auto& device = u2f_devices.front();
  // Put device in IDLE state.
  device->state_ = FidoDevice::State::kReady;

  // Manually delete connection.
  device->connection_ = nullptr;

  // Add pending transactions manually and ensure they are processed.
  TestDeviceCallbackReceiver receiver_1;
  device->pending_transactions_.emplace(U2fRequest::GetU2fVersionApduCommand(),
                                        receiver_1.callback());
  TestDeviceCallbackReceiver receiver_2;
  device->pending_transactions_.emplace(U2fRequest::GetU2fVersionApduCommand(),
                                        receiver_2.callback());
  TestDeviceCallbackReceiver receiver_3;
  device->DeviceTransact(U2fRequest::GetU2fVersionApduCommand(),
                         receiver_3.callback());

  EXPECT_EQ(FidoDevice::State::kDeviceError, device->state_);

  EXPECT_FALSE(std::get<0>(*receiver_1.result()));
  EXPECT_FALSE(std::get<0>(*receiver_2.result()));
  EXPECT_FALSE(std::get<0>(*receiver_3.result()));
}

TEST_F(FidoHidDeviceTest, TestDeviceError) {
  // Setup and enumerate mock device.
  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());

  auto hid_device = TestHidDevice();
  fake_hid_manager_->AddDevice(std::move(hid_device));
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();

  ASSERT_EQ(static_cast<size_t>(1), u2f_devices.size());
  auto& device = u2f_devices.front();

  // Mock connection where writes always fail.
  FakeHidConnection::mock_connection_error_ = true;
  device->state_ = FidoDevice::State::kReady;

  TestDeviceCallbackReceiver receiver_0;
  device->DeviceTransact(U2fRequest::GetU2fVersionApduCommand(),
                         receiver_0.callback());
  EXPECT_FALSE(std::get<0>(*receiver_0.result()));
  EXPECT_EQ(FidoDevice::State::kDeviceError, device->state_);

  // Add pending transactions manually and ensure they are processed.
  TestDeviceCallbackReceiver receiver_1;
  device->pending_transactions_.emplace(U2fRequest::GetU2fVersionApduCommand(),
                                        receiver_1.callback());
  TestDeviceCallbackReceiver receiver_2;
  device->pending_transactions_.emplace(U2fRequest::GetU2fVersionApduCommand(),
                                        receiver_2.callback());
  TestDeviceCallbackReceiver receiver_3;
  device->DeviceTransact(U2fRequest::GetU2fVersionApduCommand(),
                         receiver_3.callback());
  FakeHidConnection::mock_connection_error_ = false;

  EXPECT_EQ(FidoDevice::State::kDeviceError, device->state_);
  EXPECT_FALSE(std::get<0>(*receiver_1.result()));
  EXPECT_FALSE(std::get<0>(*receiver_2.result()));
  EXPECT_FALSE(std::get<0>(*receiver_3.result()));
}

TEST_F(FidoHidDeviceTest, TestRetryChannelAllocation) {
  const std::vector<uint8_t> kIncorrectNonce = {0x00, 0x00, 0x00, 0x00,
                                                0x00, 0x00, 0x00, 0x00};

  const std::vector<uint8_t> kChannelId = {0x01, 0x02, 0x03, 0x04};

  auto hid_device = TestHidDevice();

  // Replace device HID connection with custom client connection bound to mock
  // server-side mojo connection.
  device::mojom::HidConnectionPtr connection_client;
  MockHidConnection mock_connection(
      hid_device.Clone(), mojo::MakeRequest(&connection_client), kChannelId);

  // Delegate custom functions to be invoked for mock hid connection.
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
                CreateMockInitResponse(
                    kIncorrectNonce, mock_connection.connection_channel_id()));
          })))
      // Second response to HID_INIT request with correct nonce.
      .WillOnce(WithArg<0>(Invoke(
          [&mock_connection](device::mojom::HidConnection::ReadCallback* cb) {
            std::move(*cb).Run(true, 0,
                               CreateMockInitResponse(
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

  // Add device and set mock connection to fake hid manager.
  fake_hid_manager_->AddDeviceAndSetConnection(std::move(hid_device),
                                               std::move(connection_client));

  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  auto& device = u2f_devices.front();

  TestDeviceCallbackReceiver cb;
  device->DeviceTransact(U2fRequest::GetU2fVersionApduCommand(false),
                         cb.callback());
  cb.WaitForCallback();

  const auto& result = std::get<0>(*cb.result());
  ASSERT_TRUE(result);
  EXPECT_THAT(*result, testing::ElementsAreArray(GetValidU2fVersionResponse()));
}

}  // namespace device
