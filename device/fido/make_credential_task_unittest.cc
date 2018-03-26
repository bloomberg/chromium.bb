// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_response_test_data.h"
#include "device/fido/make_credential_task.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {

namespace {

constexpr uint8_t kClientDataHash[] = {0x01, 0x02, 0x03};
constexpr uint8_t kUserId[] = {0x01, 0x02, 0x03};
constexpr char kRpId[] = "google.com";

using TestMakeCredentialTaskCallback =
    ::device::test::StatusAndValueCallbackReceiver<
        CtapDeviceResponseCode,
        base::Optional<AuthenticatorMakeCredentialResponse>>;

}  // namespace

class FidoMakeCredentialTaskTest : public testing::Test {
 public:
  FidoMakeCredentialTaskTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

  std::unique_ptr<MakeCredentialTask> CreateMakeCredentialTask(
      FidoDevice* device) {
    PublicKeyCredentialRpEntity rp(kRpId);
    PublicKeyCredentialUserEntity user(
        std::vector<uint8_t>(std::begin(kUserId), std::end(kUserId)));
    return std::make_unique<MakeCredentialTask>(
        device,
        CtapMakeCredentialRequest(
            std::vector<uint8_t>(std::begin(kClientDataHash),
                                 std::end(kClientDataHash)),
            std::move(rp), std::move(user),
            PublicKeyCredentialParams(
                {{kU2fCredentialType,
                  base::strict_cast<int>(
                      CoseAlgorithmIdentifier::kCoseEs256)}})),
        callback_receiver_.callback());
  }

  TestMakeCredentialTaskCallback& make_credential_callback_receiver() {
    return callback_receiver_;
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestMakeCredentialTaskCallback callback_receiver_;
};

MATCHER_P(IndicatesDeviceTransactWithCommand, expected, "") {
  return !arg.empty() && (arg[0] == expected);
}

TEST_F(FidoMakeCredentialTaskTest, TestMakeCredentialSuccess) {
  auto device = std::make_unique<MockFidoDevice>();

  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);

  const auto task = CreateMakeCredentialTask(device.get());
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            make_credential_callback_receiver().status());
  EXPECT_TRUE(make_credential_callback_receiver().value());
  EXPECT_EQ(device->supported_protocol(), ProtocolVersion::kCtap);
  EXPECT_TRUE(device->device_info());
}

TEST_F(FidoMakeCredentialTaskTest, TestIncorrectAuthenticatorGetInfoResponse) {
  auto device = std::make_unique<MockFidoDevice>();

  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo, base::nullopt);

  const auto task = CreateMakeCredentialTask(device.get());
  make_credential_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            make_credential_callback_receiver().status());
  EXPECT_EQ(ProtocolVersion::kU2f, device->supported_protocol());
  EXPECT_FALSE(device->device_info());
}

}  // namespace device
