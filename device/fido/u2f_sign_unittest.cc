// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_sign.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "crypto/ec_private_key.h"
#include "crypto/sha2.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/u2f_parsing_utils.h"
#include "device/fido/virtual_fido_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {

namespace {

std::vector<uint8_t> GetTestCredentialRawIdBytes() {
  return u2f_parsing_utils::Materialize(test_data::kU2fSignKeyHandle);
}

std::vector<uint8_t> GetU2fSignCommandWithCorrectCredential() {
  return u2f_parsing_utils::Materialize(test_data::kU2fSignCommandApdu);
}

std::vector<uint8_t> GetTestSignResponse() {
  return u2f_parsing_utils::Materialize(test_data::kTestU2fSignResponse);
}

std::vector<uint8_t> GetTestAuthenticatorData() {
  return u2f_parsing_utils::Materialize(test_data::kTestSignAuthenticatorData);
}

std::vector<uint8_t> GetTestAssertionSignature() {
  return u2f_parsing_utils::Materialize(test_data::kU2fSignature);
}

std::vector<uint8_t> GetTestSignatureCounter() {
  return u2f_parsing_utils::Materialize(test_data::kTestSignatureCounter);
}

// Get a subset of the response for testing error handling.
std::vector<uint8_t> GetTestCorruptedSignResponse(size_t length) {
  DCHECK_LE(length, arraysize(test_data::kTestU2fSignResponse));
  return u2f_parsing_utils::Materialize(u2f_parsing_utils::ExtractSpan(
      test_data::kTestU2fSignResponse, 0, length));
}

using TestSignCallback = ::device::test::StatusAndValueCallbackReceiver<
    FidoReturnCode,
    base::Optional<AuthenticatorGetAssertionResponse>>;

}  // namespace

class U2fSignTest : public ::testing::Test {
 public:
  U2fSignTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

  void ForgeNextHidDiscovery() {
    discovery_ = scoped_fake_discovery_factory_.ForgeNextHidDiscovery();
  }

  std::unique_ptr<U2fSign> CreateSignRequest() {
    return CreateSignRequestWithKeys({GetTestCredentialRawIdBytes()});
  }

  std::unique_ptr<U2fSign> CreateSignRequestWithKeys(
      std::vector<std::vector<uint8_t>> registered_keys) {
    ForgeNextHidDiscovery();
    return std::make_unique<U2fSign>(
        nullptr /* connector */,
        base::flat_set<FidoTransportProtocol>(
            {FidoTransportProtocol::kUsbHumanInterfaceDevice}),
        std::move(registered_keys), challenge_parameter_,
        application_parameter_, base::nullopt,
        sign_callback_receiver_.callback());
  }

  test::FakeFidoDiscovery* discovery() const { return discovery_; }
  TestSignCallback& sign_callback_receiver() { return sign_callback_receiver_; }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::vector<uint8_t> application_parameter_ =
      u2f_parsing_utils::Materialize(test_data::kApplicationParameter);
  std::vector<uint8_t> challenge_parameter_ =
      u2f_parsing_utils::Materialize(test_data::kChallengeParameter);
  test::ScopedFakeFidoDiscoveryFactory scoped_fake_discovery_factory_;
  test::FakeFidoDiscovery* discovery_;
  TestSignCallback sign_callback_receiver_;
  base::flat_set<FidoTransportProtocol> protocols_;
};

TEST_F(U2fSignTest, TestCreateSignApduCommand) {
  const auto u2f_sign = CreateSignRequest();
  const auto encoded_sign = u2f_sign->GetU2fSignApduCommand(
      application_parameter_, GetTestCredentialRawIdBytes());
  ASSERT_TRUE(encoded_sign);
  EXPECT_THAT(*encoded_sign,
              ::testing::ElementsAreArray(test_data::kU2fSignCommandApdu));

  const auto encoded_sign_check_only = u2f_sign->GetU2fSignApduCommand(
      application_parameter_, GetTestCredentialRawIdBytes(), true);
  ASSERT_TRUE(encoded_sign_check_only);
  EXPECT_THAT(
      *encoded_sign_check_only,
      ::testing::ElementsAreArray(test_data::kU2fCheckOnlySignCommandApdu));
}

TEST_F(U2fSignTest, TestSignSuccess) {
  auto request = CreateSignRequest();
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device"));
  EXPECT_CALL(*device,
              DeviceTransactPtr(GetU2fSignCommandWithCorrectCredential(), _))
      .WillOnce(testing::Invoke(MockFidoDevice::NoErrorSign));
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device));

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, sign_callback_receiver().status());

  // Correct key was sent so a sign response is expected.
  EXPECT_EQ(GetTestAssertionSignature(),
            sign_callback_receiver().value()->signature());

  // Verify that we get the key handle used for signing.
  EXPECT_THAT(GetTestCredentialRawIdBytes(),
              ::testing::ElementsAreArray(
                  sign_callback_receiver().value()->raw_credential_id()));
}

TEST_F(U2fSignTest, TestSignSuccessWithFake) {
  auto private_key = crypto::ECPrivateKey::Create();
  std::string public_key;
  private_key->ExportRawPublicKey(&public_key);
  std::vector<uint8_t> key_handle(32);
  crypto::SHA256HashString(public_key, key_handle.data(), key_handle.size());

  std::vector<std::vector<uint8_t>> handles{key_handle};
  auto request = CreateSignRequestWithKeys(handles);
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<VirtualFidoDevice>();
  device->mutable_state()->registrations.emplace(
      key_handle, VirtualFidoDevice::RegistrationData(
                      std::move(private_key), application_parameter_, 42));
  discovery()->AddDevice(std::move(device));

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, sign_callback_receiver().status());

  // Just a sanity check, we don't verify the actual signature.
  ASSERT_GE(32u + 1u + 4u + 8u,  // Minimal ECDSA signature is 8 bytes
            sign_callback_receiver()
                .value()
                ->auth_data()
                .SerializeToByteArray()
                .size());
  EXPECT_EQ(0x01,
            sign_callback_receiver()
                .value()
                ->auth_data()
                .SerializeToByteArray()[32]);  // UP flag
  EXPECT_EQ(43, sign_callback_receiver()
                    .value()
                    ->auth_data()
                    .SerializeToByteArray()[36]);  // counter
}

TEST_F(U2fSignTest, TestDelayedSuccess) {
  auto request = CreateSignRequest();
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  // Go through the state machine twice before success.
  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  EXPECT_CALL(*device,
              DeviceTransactPtr(GetU2fSignCommandWithCorrectCredential(), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NotSatisfied))
      .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorSign));
  EXPECT_CALL(*device, TryWinkRef(_))
      .Times(2)
      .WillRepeatedly(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device));

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, sign_callback_receiver().status());

  // Correct key was sent so a sign response is expected.
  EXPECT_EQ(GetTestAssertionSignature(),
            sign_callback_receiver().value()->signature());

  // Verify that we get the key handle used for signing.
  EXPECT_THAT(GetTestCredentialRawIdBytes(),
              ::testing::ElementsAreArray(
                  sign_callback_receiver().value()->raw_credential_id()));
}

TEST_F(U2fSignTest, TestMultipleHandles) {
  // Two wrong keys followed by a correct key ensuring the wrong keys will be
  // tested first.
  const auto correct_key_handle = GetTestCredentialRawIdBytes();
  auto request = CreateSignRequestWithKeys(
      {u2f_parsing_utils::Materialize(test_data::kKeyHandleAlpha),
       u2f_parsing_utils::Materialize(test_data::kKeyHandleBeta),
       correct_key_handle});
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockFidoDevice>();
  // Wrong key would respond with SW_WRONG_DATA.
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  EXPECT_CALL(*device,
              DeviceTransactPtr(u2f_parsing_utils::Materialize(
                                    test_data::kU2fSignCommandApduWithKeyAlpha),
                                _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));
  EXPECT_CALL(*device,
              DeviceTransactPtr(u2f_parsing_utils::Materialize(
                                    test_data::kU2fSignCommandApduWithKeyBeta),
                                _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(*device,
              DeviceTransactPtr(GetU2fSignCommandWithCorrectCredential(), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorSign));

  // Only one wink expected per device.
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device));

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, sign_callback_receiver().status());

  // Correct key was sent so a sign response is expected.
  EXPECT_EQ(GetTestAssertionSignature(),
            sign_callback_receiver().value()->signature());

  // Verify that we get the key handle used for signing.
  EXPECT_EQ(correct_key_handle,
            sign_callback_receiver().value()->raw_credential_id());
}

TEST_F(U2fSignTest, TestMultipleDevices) {
  const auto correct_key_handle = GetTestCredentialRawIdBytes();
  auto request = CreateSignRequestWithKeys(
      {GetTestCredentialRawIdBytes(),
       u2f_parsing_utils::Materialize(test_data::kKeyHandleAlpha)});
  request->Start();

  auto device0 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(::testing::Return("device0"));
  EXPECT_CALL(*device0,
              DeviceTransactPtr(GetU2fSignCommandWithCorrectCredential(), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));
  EXPECT_CALL(*device0,
              DeviceTransactPtr(u2f_parsing_utils::Materialize(
                                    test_data::kU2fSignCommandApduWithKeyAlpha),
                                _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NotSatisfied));
  // One wink per device.
  EXPECT_CALL(*device0, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device0));

  // Second device will have a successful touch.
  auto device1 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(::testing::Return("device1"));
  EXPECT_CALL(*device1,
              DeviceTransactPtr(GetU2fSignCommandWithCorrectCredential(), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorSign));

  // One wink per device.
  EXPECT_CALL(*device1, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device1));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, sign_callback_receiver().status());

  // Correct key was sent so a sign response is expected.
  EXPECT_EQ(GetTestAssertionSignature(),
            sign_callback_receiver().value()->signature());

  // Verify that we get the key handle used for signing.
  EXPECT_EQ(correct_key_handle,
            sign_callback_receiver().value()->raw_credential_id());
}

TEST_F(U2fSignTest, TestFakeEnroll) {
  auto request = CreateSignRequestWithKeys(
      {u2f_parsing_utils::Materialize(test_data::kKeyHandleAlpha),
       u2f_parsing_utils::Materialize(test_data::kKeyHandleBeta)});
  request->Start();

  auto device0 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device0,
              DeviceTransactPtr(u2f_parsing_utils::Materialize(
                                    test_data::kU2fSignCommandApduWithKeyAlpha),
                                _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(*device0,
              DeviceTransactPtr(u2f_parsing_utils::Materialize(
                                    test_data::kU2fSignCommandApduWithKeyBeta),
                                _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NotSatisfied));
  // One wink per device.
  EXPECT_CALL(*device0, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device0));

  // Second device will be have a successful touch.
  auto device1 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(::testing::Return("device1"));
  // Both keys will be tried, when both fail, register is tried on that device.
  EXPECT_CALL(*device1,
              DeviceTransactPtr(u2f_parsing_utils::Materialize(
                                    test_data::kU2fSignCommandApduWithKeyAlpha),
                                _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));
  EXPECT_CALL(*device1,
              DeviceTransactPtr(u2f_parsing_utils::Materialize(
                                    test_data::kU2fSignCommandApduWithKeyBeta),
                                _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));
  EXPECT_CALL(*device1,
              DeviceTransactPtr(u2f_parsing_utils::Materialize(
                                    test_data::kU2fFakeRegisterCommand),
                                _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorRegister));

  // One wink per device.
  EXPECT_CALL(*device1, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device1));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  sign_callback_receiver().WaitForCallback();
  // Device that responded had no correct keys.
  EXPECT_EQ(FidoReturnCode::kUserConsentButCredentialNotRecognized,
            sign_callback_receiver().status());
  EXPECT_FALSE(sign_callback_receiver().value());
}

TEST_F(U2fSignTest, TestFakeEnrollErroringOut) {
  auto request = CreateSignRequest();
  request->Start();
  // First device errors out on all requests (including the sign request and
  // fake registration attempt). The device should then be abandoned to prevent
  // the test from crashing or timing out.
  auto device0 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(::testing::Return("device0"));
  EXPECT_CALL(*device0,
              DeviceTransactPtr(GetU2fSignCommandWithCorrectCredential(), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));
  EXPECT_CALL(*device0,
              DeviceTransactPtr(u2f_parsing_utils::Materialize(
                                    test_data::kU2fFakeRegisterCommand),
                                _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));
  // One wink per device.
  EXPECT_CALL(*device0, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device0));

  // Second device will have a successful touch and sign on the first attempt.
  auto device1 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(::testing::Return("device1"));
  EXPECT_CALL(*device1,
              DeviceTransactPtr(GetU2fSignCommandWithCorrectCredential(), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorSign));
  // One wink per device.
  EXPECT_CALL(*device1, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device1));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  // Correct key was sent so a sign response is expected.
  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(GetTestAssertionSignature(),
            sign_callback_receiver().value()->signature());
}

TEST_F(U2fSignTest, TestAuthenticatorDataForSign) {
  constexpr uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence);

  EXPECT_EQ(GetTestAuthenticatorData(),
            AuthenticatorData(u2f_parsing_utils::Materialize(
                                  test_data::kApplicationParameter),
                              flags, GetTestSignatureCounter(), base::nullopt)
                .SerializeToByteArray());
}

TEST_F(U2fSignTest, TestSignResponseData) {
  base::Optional<AuthenticatorGetAssertionResponse> response =
      AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
          u2f_parsing_utils::Materialize(test_data::kApplicationParameter),
          GetTestSignResponse(), GetTestCredentialRawIdBytes());
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(GetTestCredentialRawIdBytes(), response->raw_credential_id());
  EXPECT_EQ(GetTestAuthenticatorData(),
            response->auth_data().SerializeToByteArray());
  EXPECT_EQ(GetTestAssertionSignature(), response->signature());
}

TEST_F(U2fSignTest, TestNullKeyHandle) {
  base::Optional<AuthenticatorGetAssertionResponse> response =
      AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
          u2f_parsing_utils::Materialize(test_data::kApplicationParameter),
          GetTestSignResponse(), std::vector<uint8_t>());
  EXPECT_FALSE(response);
}

TEST_F(U2fSignTest, TestNullResponse) {
  base::Optional<AuthenticatorGetAssertionResponse> response =
      AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
          u2f_parsing_utils::Materialize(test_data::kApplicationParameter),
          std::vector<uint8_t>(), GetTestCredentialRawIdBytes());
  EXPECT_FALSE(response);
}

TEST_F(U2fSignTest, TestCorruptedCounter) {
  // A sign response of less than 5 bytes.
  base::Optional<AuthenticatorGetAssertionResponse> response =
      AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
          u2f_parsing_utils::Materialize(test_data::kApplicationParameter),
          GetTestCorruptedSignResponse(3), GetTestCredentialRawIdBytes());
  EXPECT_FALSE(response);
}

TEST_F(U2fSignTest, TestCorruptedSignature) {
  // A sign response no more than 5 bytes.
  base::Optional<AuthenticatorGetAssertionResponse> response =
      AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
          u2f_parsing_utils::Materialize(test_data::kApplicationParameter),
          GetTestCorruptedSignResponse(5), GetTestCredentialRawIdBytes());
  EXPECT_FALSE(response);
}

// Device returns success, but the response is unparse-able.
TEST_F(U2fSignTest, TestSignWithCorruptedResponse) {
  auto request = CreateSignRequest();
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  EXPECT_CALL(*device,
              DeviceTransactPtr(GetU2fSignCommandWithCorrectCredential(), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::SignWithCorruptedResponse));
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device));

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kAuthenticatorResponseInvalid,
            sign_callback_receiver().status());
  EXPECT_FALSE(sign_callback_receiver().value());
}

MATCHER_P(WithApplicationParameter, expected, "") {
  // See
  // https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#authentication-request-message---u2f_authenticate
  if (arg.size() < 71) {
    return false;
  }

  base::span<const uint8_t> cmd_bytes(arg);
  auto application_parameter = cmd_bytes.subspan(
      4 /* framing bytes */ + 3 /* request length */ + 32 /* challenge hash */,
      32);
  return std::equal(application_parameter.begin(), application_parameter.end(),
                    expected.begin());
}

TEST_F(U2fSignTest, TestAlternativeApplicationParameter) {
  const std::vector<uint8_t> signing_key_handle(32, 0x0A);
  const std::vector<uint8_t> primary_app_param(32, 1);
  const std::vector<uint8_t> alt_app_param(32, 2);

  ForgeNextHidDiscovery();
  auto request = std::make_unique<U2fSign>(
      nullptr /* connector */,
      base::flat_set<FidoTransportProtocol>(
          {FidoTransportProtocol::kUsbHumanInterfaceDevice}),
      std::vector<std::vector<uint8_t>>({signing_key_handle}),
      std::vector<uint8_t>(32), primary_app_param, alt_app_param,
      sign_callback_receiver_.callback());
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  // The first request will use the primary app_param, which will be rejected.
  EXPECT_CALL(*device,
              DeviceTransactPtr(WithApplicationParameter(primary_app_param), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));
  // After the rejection, the alternative should be tried.
  EXPECT_CALL(*device,
              DeviceTransactPtr(WithApplicationParameter(alt_app_param), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorSign));
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device));

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, sign_callback_receiver().status());

  EXPECT_EQ(GetTestAssertionSignature(),
            sign_callback_receiver().value()->signature());
  EXPECT_EQ(signing_key_handle,
            sign_callback_receiver().value()->raw_credential_id());
}

}  // namespace device
