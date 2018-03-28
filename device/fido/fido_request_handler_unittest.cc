// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/test/scoped_task_environment.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_response_test_data.h"
#include "device/fido/make_credential_request_handler.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/u2f_parsing_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {

namespace {

constexpr uint8_t kClientDataHash[] = {0x01, 0x02, 0x03};
constexpr uint8_t kUserId[] = {0x01, 0x02, 0x03};
constexpr char kRpId[] = "google.com";

using TestMakeCredentialRequestCallback =
    ::device::test::StatusAndValueCallbackReceiver<
        FidoReturnCode,
        base::Optional<AuthenticatorMakeCredentialResponse>>;

}  // namespace

class FidoRequestHandlerTest : public ::testing::Test {
 public:
  void ForgeNextHidDiscovery() {
    discovery_ = scoped_fake_discovery_factory_.ForgeNextHidDiscovery();
  }

  std::unique_ptr<MakeCredentialRequestHandler> CreateMakeCredentialHandler() {
    ForgeNextHidDiscovery();
    PublicKeyCredentialRpEntity rp(kRpId);
    PublicKeyCredentialUserEntity user(u2f_parsing_utils::Materialize(kUserId));
    PublicKeyCredentialParams credential_params({{kU2fCredentialType}});

    auto request_parameter = CtapMakeCredentialRequest(
        u2f_parsing_utils::Materialize(kClientDataHash), std::move(rp),
        std::move(user), std::move(credential_params));

    return std::make_unique<MakeCredentialRequestHandler>(
        nullptr,
        base::flat_set<U2fTransportProtocol>(
            {U2fTransportProtocol::kUsbHumanInterfaceDevice}),
        std::move(request_parameter), cb_.callback());
  }

  test::FakeFidoDiscovery* discovery() const { return discovery_; }
  TestMakeCredentialRequestCallback& callback() { return cb_; }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME};
  test::ScopedFakeFidoDiscoveryFactory scoped_fake_discovery_factory_;
  test::FakeFidoDiscovery* discovery_;
  TestMakeCredentialRequestCallback cb_;
};

TEST_F(FidoRequestHandlerTest, TestMakeCredentialRequestHandler) {
  auto request_handler = CreateMakeCredentialHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device0"));
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);

  discovery()->AddDevice(std::move(device));
  callback().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, callback().status());
  EXPECT_TRUE(request_handler->is_complete());
}

// Test a scenario where 2 devices are connected and a response is received from
// only a single device(device1) and the remaining device hangs.
TEST_F(FidoRequestHandlerTest, TestRequestWithMultipleDevices) {
  auto request_handler = CreateMakeCredentialHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  // Represents a connected device that hangs without a response.
  auto device0 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  device0->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  device0->ExpectCtap2CommandWithoutResponse(
      CtapRequestCommand::kAuthenticatorMakeCredential);
  device0->ExpectCtap2CommandWithoutResponse(
      CtapRequestCommand::kAuthenticatorCancel);

  // Represents a connected device that response successfully.
  auto device1 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  device1->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  device1->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);

  discovery()->AddDevice(std::move(device0));
  discovery()->AddDevice(std::move(device1));

  callback().WaitForCallback();
  EXPECT_TRUE(request_handler->is_complete());
  EXPECT_EQ(FidoReturnCode::kSuccess, callback().status());
}

// Test a scenario where 2 devices respond successfully with small time delay.
// Only the first received response should be passed on to the relying party,
// and cancel request should be sent to the other authenticator.
TEST_F(FidoRequestHandlerTest, TestRequestWithMultipleSuccessResponses) {
  auto request_handler = CreateMakeCredentialHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  // Represents a connected device that responds successfully after small time
  // delay.
  auto device0 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  device0->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  device0->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse,
      base::TimeDelta::FromMicroseconds(1));

  // Represents a device that returns a success response after a longer time
  // delay.
  auto device1 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  device1->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  device1->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse,
      base::TimeDelta::FromMicroseconds(10));
  device1->ExpectCtap2CommandWithoutResponse(
      CtapRequestCommand::kAuthenticatorCancel);

  discovery()->AddDevice(std::move(device0));
  discovery()->AddDevice(std::move(device1));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  callback().WaitForCallback();
  EXPECT_TRUE(request_handler->is_complete());
  EXPECT_EQ(FidoReturnCode::kSuccess, callback().status());
}

// Test a scenario where 3 devices respond with a processing error, an UP(user
// presence) verified failure response with small time delay, and an UP verified
// failure response with big time delay, respectively. Request for device with
// processing error should be immediately dropped. Also, for UP verified
// failures, the first received response should be passed on to the relying
// party and cancel command should be sent to the remaining device.
TEST_F(FidoRequestHandlerTest, TestRequestWithMultipleFailureResponses) {
  auto request_handler = CreateMakeCredentialHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  // Represents a connected device that immediately responds with processing
  // error.
  auto device0 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  device0->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  device0->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential, base::nullopt);

  // Represents a device that returns UP verified failure response after a small
  // time delay.
  auto device1 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  device1->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  device1->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      std::vector<uint8_t>{base::strict_cast<uint8_t>(
          CtapDeviceResponseCode::kCtap2ErrInvalidCredential)},
      base::TimeDelta::FromMicroseconds(1));

  // Represents a device that returns UP verified failure response after a big
  // time delay.
  auto device2 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device2, GetId()).WillRepeatedly(testing::Return("device2"));
  device2->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  device2->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      std::vector<uint8_t>{base::strict_cast<uint8_t>(
          CtapDeviceResponseCode::kCtap2ErrInvalidCredential)},
      base::TimeDelta::FromMicroseconds(10));
  device2->ExpectCtap2CommandWithoutResponse(
      CtapRequestCommand::kAuthenticatorCancel);

  discovery()->AddDevice(std::move(device0));
  discovery()->AddDevice(std::move(device1));
  discovery()->AddDevice(std::move(device2));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  callback().WaitForCallback();
  EXPECT_TRUE(request_handler->is_complete());
  EXPECT_EQ(FidoReturnCode::kConditionsNotSatisfied, callback().status());
}

}  // namespace device
