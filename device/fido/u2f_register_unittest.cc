// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_register.h"

#include <iterator>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "components/cbor/cbor_writer.h"
#include "device/fido/attestation_object.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ec_public_key.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_attestation_statement.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/u2f_parsing_utils.h"
#include "device/fido/virtual_fido_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::testing::_;

namespace {

constexpr bool kNoIndividualAttestation = false;
constexpr bool kIndividualAttestation = true;

// The attested credential data, excluding the public key bytes. Append
// with kTestECPublicKeyCOSE to get the complete attestation data.
constexpr uint8_t kTestAttestedCredentialDataPrefix[] = {
    // clang-format off
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,  // 16-byte aaguid
    0x00, 0x40,  // 2-byte length
    0x3E, 0xBD, 0x89, 0xBF, 0x77, 0xEC, 0x50, 0x97, 0x55, 0xEE, 0x9C,
    0x26, 0x35, 0xEF, 0xAA, 0xAC, 0x7B, 0x2B, 0x9C, 0x5C, 0xEF, 0x17,
    0x36, 0xC3, 0x71, 0x7D, 0xA4, 0x85, 0x34, 0xC8, 0xC6, 0xB6, 0x54,
    0xD7, 0xFF, 0x94, 0x5F, 0x50, 0xB5, 0xCC, 0x4E, 0x78, 0x05, 0x5B,
    0xDD, 0x39, 0x6B, 0x64, 0xF7, 0x8D, 0xA2, 0xC5, 0xF9, 0x62, 0x00,
    0xCC, 0xD4, 0x15, 0xCD, 0x08, 0xFE, 0x42, 0x00, 0x38,  // 64-byte key handle
    // clang-format on
};

// The authenticator data, excluding the attested credential data bytes. Append
// with attested credential data to get the complete authenticator data.
constexpr uint8_t kTestAuthenticatorDataPrefix[] = {
    // clang-format off
    // sha256 hash of rp id.
    0x11, 0x94, 0x22, 0x8D, 0xA8, 0xFD, 0xBD, 0xEE, 0xFD, 0x26, 0x1B,
    0xD7, 0xB6, 0x59, 0x5C, 0xFD, 0x70, 0xA5, 0x0D, 0x70, 0xC6, 0x40,
    0x7B, 0xCF, 0x01, 0x3D, 0xE9, 0x6D, 0x4E, 0xFB, 0x17, 0xDE,
    0x41,  // flags (TUP and AT bits set)
    0x00, 0x00, 0x00, 0x00  // counter
    // clang-format on
};

// Components of the CBOR needed to form an authenticator object.
// Combined diagnostic notation:
// {"fmt": "fido-u2f", "attStmt": {"sig": h'30...}, "authData": h'D4C9D9...'}
constexpr uint8_t kFormatFidoU2fCBOR[] = {
    // clang-format off
    0xA3,  // map(3)
      0x63,  // text(3)
        0x66, 0x6D, 0x74,  // "fmt"
      0x68,  // text(8)
        0x66, 0x69, 0x64, 0x6F, 0x2D, 0x75, 0x32, 0x66  // "fido-u2f"
    // clang-format on
};

constexpr uint8_t kAttStmtCBOR[] = {
    // clang-format off
      0x67,  // text(7)
        0x61, 0x74, 0x74, 0x53, 0x74, 0x6D, 0x74  // "attStmt"
    // clang-format on
};

constexpr uint8_t kAuthDataCBOR[] = {
    // clang-format off
    0x68,  // text(8)
      0x61, 0x75, 0x74, 0x68, 0x44, 0x61, 0x74, 0x61,  // "authData"
    0x58, 0xC4  // bytes(196). i.e.,the authenticator_data bytearray
    // clang-format on
};

// Helpers for testing U2f register responses.
std::vector<uint8_t> GetTestECPublicKeyCOSE() {
  return u2f_parsing_utils::Materialize(test_data::kTestECPublicKeyCOSE);
}

std::vector<uint8_t> GetTestRegisterResponse() {
  return u2f_parsing_utils::Materialize(test_data::kTestU2fRegisterResponse);
}

std::vector<uint8_t> GetTestRegisterRequest() {
  return u2f_parsing_utils::Materialize(test_data::kU2fRegisterCommandApdu);
}

std::vector<uint8_t> GetTestCredentialRawIdBytes() {
  return u2f_parsing_utils::Materialize(test_data::kU2fSignKeyHandle);
}

std::vector<uint8_t> GetU2fAttestationStatementCBOR() {
  return u2f_parsing_utils::Materialize(
      test_data::kU2fAttestationStatementCBOR);
}

std::vector<uint8_t> GetTestAttestedCredentialDataBytes() {
  // Combine kTestAttestedCredentialDataPrefix and kTestECPublicKeyCOSE.
  auto test_attested_data =
      u2f_parsing_utils::Materialize(kTestAttestedCredentialDataPrefix);
  test_attested_data.insert(test_attested_data.end(),
                            std::begin(test_data::kTestECPublicKeyCOSE),
                            std::end(test_data::kTestECPublicKeyCOSE));
  return test_attested_data;
}

std::vector<uint8_t> GetTestAuthenticatorDataBytes() {
  // Build the test authenticator data.
  auto test_authenticator_data =
      u2f_parsing_utils::Materialize(kTestAuthenticatorDataPrefix);
  std::vector<uint8_t> test_attested_data =
      GetTestAttestedCredentialDataBytes();
  test_authenticator_data.insert(test_authenticator_data.end(),
                                 test_attested_data.begin(),
                                 test_attested_data.end());
  return test_authenticator_data;
}

std::vector<uint8_t> GetTestAttestationObjectBytes() {
  auto test_authenticator_object =
      u2f_parsing_utils::Materialize(kFormatFidoU2fCBOR);
  test_authenticator_object.insert(test_authenticator_object.end(),
                                   std::begin(kAttStmtCBOR),
                                   std::end(kAttStmtCBOR));
  test_authenticator_object.insert(
      test_authenticator_object.end(),
      std::begin(test_data::kU2fAttestationStatementCBOR),
      std::end(test_data::kU2fAttestationStatementCBOR));
  test_authenticator_object.insert(test_authenticator_object.end(),
                                   std::begin(kAuthDataCBOR),
                                   std::end(kAuthDataCBOR));
  std::vector<uint8_t> test_authenticator_data =
      GetTestAuthenticatorDataBytes();
  test_authenticator_object.insert(test_authenticator_object.end(),
                                   test_authenticator_data.begin(),
                                   test_authenticator_data.end());
  return test_authenticator_object;
}

using TestRegisterCallback = ::device::test::StatusAndValueCallbackReceiver<
    FidoReturnCode,
    base::Optional<AuthenticatorMakeCredentialResponse>>;

}  // namespace

class U2fRegisterTest : public ::testing::Test {
 public:
  void ForgeNextHidDiscovery() {
    discovery_ = scoped_fake_discovery_factory_.ForgeNextHidDiscovery();
  }

  std::unique_ptr<U2fRegister> CreateRegisterRequest() {
    return CreateRegisterRequestWithRegisteredKeys(
        std::vector<std::vector<uint8_t>>());
  }

  // Creates a U2F register request with `none` attestation, and the given
  // previously |registered_keys|.
  std::unique_ptr<U2fRegister> CreateRegisterRequestWithRegisteredKeys(
      std::vector<std::vector<uint8_t>> registered_keys) {
    ForgeNextHidDiscovery();
    return std::make_unique<U2fRegister>(
        nullptr /* connector */,
        base::flat_set<FidoTransportProtocol>(
            {FidoTransportProtocol::kUsbHumanInterfaceDevice}),
        registered_keys,
        u2f_parsing_utils::Materialize(test_data::kChallengeParameter),
        u2f_parsing_utils::Materialize(test_data::kApplicationParameter),
        kNoIndividualAttestation, register_callback_receiver_.callback());
  }

  test::FakeFidoDiscovery* discovery() const { return discovery_; }
  TestRegisterCallback& register_callback_receiver() {
    return register_callback_receiver_;
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::vector<uint8_t> application_parameter_ =
      u2f_parsing_utils::Materialize(test_data::kApplicationParameter);
  std::vector<uint8_t> challenge_parameter_ =
      u2f_parsing_utils::Materialize(test_data::kChallengeParameter);
  std::vector<std::vector<uint8_t>> key_handles_;
  base::flat_set<FidoTransportProtocol> protocols_;
  test::ScopedFakeFidoDiscoveryFactory scoped_fake_discovery_factory_;
  test::FakeFidoDiscovery* discovery_;
  TestRegisterCallback register_callback_receiver_;
};

TEST_F(U2fRegisterTest, TestCreateU2fRegisterCommand) {
  U2fRegister register_request(nullptr /* connector */, protocols_,
                               key_handles_, challenge_parameter_,
                               application_parameter_, kNoIndividualAttestation,
                               register_callback_receiver().callback());

  const auto register_command_without_individual_attestation =
      register_request.GetU2fRegisterApduCommand(kNoIndividualAttestation);

  ASSERT_TRUE(register_command_without_individual_attestation);
  EXPECT_THAT(*register_command_without_individual_attestation,
              ::testing::ElementsAreArray(test_data::kU2fRegisterCommandApdu));

  const auto register_command_with_individual_attestation =
      register_request.GetU2fRegisterApduCommand(kIndividualAttestation);
  ASSERT_TRUE(register_command_with_individual_attestation);
  EXPECT_THAT(*register_command_with_individual_attestation,
              ::testing::ElementsAreArray(
                  test_data::kU2fRegisterCommandApduWithIndividualAttestation));
}

TEST_F(U2fRegisterTest, TestCreateRegisterWithIncorrectParameters) {
  std::vector<uint8_t> application_parameter(kU2fParameterLength, 0x01);
  std::vector<uint8_t> challenge_parameter(kU2fParameterLength, 0xff);

  U2fRegister register_request(nullptr, protocols_, key_handles_,
                               challenge_parameter, application_parameter,
                               kNoIndividualAttestation,
                               register_callback_receiver().callback());
  const auto register_without_individual_attestation =
      register_request.GetU2fRegisterApduCommand(kNoIndividualAttestation);

  ASSERT_TRUE(register_without_individual_attestation);
  ASSERT_LE(3u, register_without_individual_attestation->size());
  // Individual attestation bit should be cleared.
  EXPECT_EQ(0, (*register_without_individual_attestation)[2] & 0x80);

  const auto register_request_with_individual_attestation =
      register_request.GetU2fRegisterApduCommand(kIndividualAttestation);
  ASSERT_TRUE(register_request_with_individual_attestation);
  ASSERT_LE(3u, register_request_with_individual_attestation->size());
  // Individual attestation bit should be set.
  EXPECT_EQ(0x80, (*register_request_with_individual_attestation)[2] & 0x80);

  // Expect null result with incorrectly sized application_parameter.
  application_parameter.push_back(0xff);
  auto incorrect_register_cmd =
      U2fRegister(nullptr, protocols_, key_handles_, challenge_parameter,
                  application_parameter, kNoIndividualAttestation,
                  register_callback_receiver().callback())
          .GetU2fRegisterApduCommand(kNoIndividualAttestation);
  EXPECT_FALSE(incorrect_register_cmd);
  application_parameter.pop_back();

  // Expect null result with incorrectly sized challenge.
  challenge_parameter.push_back(0xff);
  incorrect_register_cmd =
      U2fRegister(nullptr, protocols_, key_handles_, challenge_parameter,
                  application_parameter, kNoIndividualAttestation,
                  register_callback_receiver().callback())
          .GetU2fRegisterApduCommand(kNoIndividualAttestation);
  EXPECT_FALSE(incorrect_register_cmd);
}

TEST_F(U2fRegisterTest, TestRegisterSuccess) {
  auto request = CreateRegisterRequest();
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device"));
  EXPECT_CALL(*device, DeviceTransactPtr(GetTestRegisterRequest(), _))
      .WillOnce(testing::Invoke(MockFidoDevice::NoErrorRegister));
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device));

  register_callback_receiver().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, register_callback_receiver().status());
  ASSERT_TRUE(register_callback_receiver().value());
  EXPECT_EQ(GetTestCredentialRawIdBytes(),
            register_callback_receiver().value()->raw_credential_id());
}

TEST_F(U2fRegisterTest, TestRegisterSuccessWithFake) {
  auto request = CreateRegisterRequest();
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<VirtualFidoDevice>();
  discovery()->AddDevice(std::move(device));

  register_callback_receiver().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, register_callback_receiver().status());

  // We don't verify the response from the fake, but do a quick sanity check.
  ASSERT_TRUE(register_callback_receiver().value());
  EXPECT_EQ(32ul,
            register_callback_receiver().value()->raw_credential_id().size());
}

TEST_F(U2fRegisterTest, TestDelayedSuccess) {
  auto request = CreateRegisterRequest();
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device"));
  // Go through the state machine twice before success.
  EXPECT_CALL(*device, DeviceTransactPtr(GetTestRegisterRequest(), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NotSatisfied))
      .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorRegister));
  EXPECT_CALL(*device, TryWinkRef(_))
      .Times(2)
      .WillRepeatedly(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device));

  register_callback_receiver().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, register_callback_receiver().status());
  ASSERT_TRUE(register_callback_receiver().value());
  EXPECT_EQ(GetTestCredentialRawIdBytes(),
            register_callback_receiver().value()->raw_credential_id());
}

TEST_F(U2fRegisterTest, TestMultipleDevices) {
  auto request = CreateRegisterRequest();
  request->Start();

  auto device0 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(::testing::Return("device0"));
  EXPECT_CALL(*device0, DeviceTransactPtr(GetTestRegisterRequest(), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NotSatisfied));
  // One wink per device.
  EXPECT_CALL(*device0, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device0));

  // Second device will have a successful touch.
  auto device1 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(::testing::Return("device1"));
  EXPECT_CALL(*device1, DeviceTransactPtr(GetTestRegisterRequest(), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorRegister));
  // One wink per device.
  EXPECT_CALL(*device1, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device1));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  register_callback_receiver().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, register_callback_receiver().status());
  ASSERT_TRUE(register_callback_receiver().value());
  EXPECT_EQ(GetTestCredentialRawIdBytes(),
            register_callback_receiver().value()->raw_credential_id());
}

// Tests a scenario where a single device is connected and registration call
// is received with three unknown key handles. We expect that three check only
// sign-in calls be processed before registration.
TEST_F(U2fRegisterTest, TestSingleDeviceRegistrationWithExclusionList) {
  // Simulate three unknown key handles.
  auto request = CreateRegisterRequestWithRegisteredKeys(
      {u2f_parsing_utils::Materialize(test_data::kKeyHandleAlpha),
       u2f_parsing_utils::Materialize(test_data::kKeyHandleBeta)});
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  // DeviceTransact() will be called three times including two check
  // only sign-in calls and one registration call. For the first two calls,
  // device will invoke MockFidoDevice::WrongData as the authenticator did not
  // create the two key handles provided in the exclude list. At the third
  // call, MockFidoDevice::NoErrorRegister will be invoked after registration.
  EXPECT_CALL(*device.get(),
              DeviceTransactPtr(
                  u2f_parsing_utils::Materialize(
                      test_data::kU2fCheckOnlySignCommandApduWithKeyAlpha),
                  _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(
      *device.get(),
      DeviceTransactPtr(u2f_parsing_utils::Materialize(
                            test_data::kU2fCheckOnlySignCommandApduWithKeyBeta),
                        _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(*device.get(), DeviceTransactPtr(GetTestRegisterRequest(), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorRegister));

  // TryWink() will be called twice. First during the check only sign-in. After
  // check only sign operation is complete, request state is changed to IDLE,
  // and TryWink() is called again before Register() is called.
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device));

  register_callback_receiver().WaitForCallback();
  ASSERT_TRUE(register_callback_receiver().value());
  EXPECT_EQ(FidoReturnCode::kSuccess, register_callback_receiver().status());
  EXPECT_EQ(GetTestCredentialRawIdBytes(),
            register_callback_receiver().value()->raw_credential_id());
}

// Tests a scenario where two devices are connected and registration call is
// received with three unknown key handles. We assume that user will proceed the
// registration with second device, "device1".
TEST_F(U2fRegisterTest, TestMultipleDeviceRegistrationWithExclusionList) {
  // Simulate three unknown key handles.
  auto request = CreateRegisterRequestWithRegisteredKeys(
      {u2f_parsing_utils::Materialize(test_data::kKeyHandleAlpha),
       u2f_parsing_utils::Materialize(test_data::kKeyHandleBeta),
       u2f_parsing_utils::Materialize(test_data::kKeyHandleGamma)});
  request->Start();

  auto device0 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(::testing::Return("device0"));
  // DeviceTransact() will be called four times: three times to check for
  // duplicate key handles and once for registration. Since user
  // will register using "device1", the fourth call will invoke
  // MockFidoDevice::NotSatisfied.
  EXPECT_CALL(*device0.get(),
              DeviceTransactPtr(
                  u2f_parsing_utils::Materialize(
                      test_data::kU2fCheckOnlySignCommandApduWithKeyAlpha),
                  _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(
      *device0.get(),
      DeviceTransactPtr(u2f_parsing_utils::Materialize(
                            test_data::kU2fCheckOnlySignCommandApduWithKeyBeta),
                        _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(*device0.get(),
              DeviceTransactPtr(
                  u2f_parsing_utils::Materialize(
                      test_data::kU2fCheckOnlySignCommandApduWithKeyGamma),
                  _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(*device0.get(), DeviceTransactPtr(GetTestRegisterRequest(), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NotSatisfied));

  // TryWink() will be called twice on both devices -- during check only
  // sign-in operation and during registration attempt.
  EXPECT_CALL(*device0, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device0));

  auto device1 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  // We assume that user registers with second device. Therefore, the fourth
  // DeviceTransact() will invoke MockFidoDevice::NoErrorRegister after
  // successful registration.
  EXPECT_CALL(*device1.get(),
              DeviceTransactPtr(
                  u2f_parsing_utils::Materialize(
                      test_data::kU2fCheckOnlySignCommandApduWithKeyAlpha),
                  _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(
      *device1.get(),
      DeviceTransactPtr(u2f_parsing_utils::Materialize(
                            test_data::kU2fCheckOnlySignCommandApduWithKeyBeta),
                        _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(*device1.get(),
              DeviceTransactPtr(
                  u2f_parsing_utils::Materialize(
                      test_data::kU2fCheckOnlySignCommandApduWithKeyGamma),
                  _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(*device1.get(), DeviceTransactPtr(GetTestRegisterRequest(), _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorRegister));

  // TryWink() will be called twice on both devices -- during check only
  // sign-in operation and during registration attempt.
  EXPECT_CALL(*device1, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device1));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  register_callback_receiver().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, register_callback_receiver().status());
  ASSERT_TRUE(register_callback_receiver().value());
  EXPECT_EQ(GetTestCredentialRawIdBytes(),
            register_callback_receiver().value()->raw_credential_id());
}

// Tests a scenario where single device is connected and registration is called
// with a key in the exclude list that was created by this device. We assume
// that the duplicate key is the last key handle in the exclude list. Therefore,
// after duplicate key handle is found, the process is expected to terminate
// after calling bogus registration which checks for user presence.
TEST_F(U2fRegisterTest, TestSingleDeviceRegistrationWithDuplicateHandle) {
  // Simulate three unknown key handles followed by a duplicate key.
  auto request = CreateRegisterRequestWithRegisteredKeys(
      {u2f_parsing_utils::Materialize(test_data::kKeyHandleAlpha),
       u2f_parsing_utils::Materialize(test_data::kKeyHandleBeta),
       u2f_parsing_utils::Materialize(test_data::kKeyHandleGamma)});
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  // For four keys in exclude list, the first three keys will invoke
  // MockFidoDevice::WrongData and the final duplicate key handle will invoke
  // MockFidoDevice::NoErrorSign. Once duplicate key handle is found, bogus
  // registration is called to confirm user presence. This invokes
  // MockFidoDevice::NoErrorRegister.
  EXPECT_CALL(*device.get(),
              DeviceTransactPtr(
                  u2f_parsing_utils::Materialize(
                      test_data::kU2fCheckOnlySignCommandApduWithKeyAlpha),
                  _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(
      *device.get(),
      DeviceTransactPtr(u2f_parsing_utils::Materialize(
                            test_data::kU2fCheckOnlySignCommandApduWithKeyBeta),
                        _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(*device.get(),
              DeviceTransactPtr(
                  u2f_parsing_utils::Materialize(
                      test_data::kU2fCheckOnlySignCommandApduWithKeyGamma),
                  _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorSign));

  EXPECT_CALL(*device.get(),
              DeviceTransactPtr(u2f_parsing_utils::Materialize(
                                    test_data::kU2fFakeRegisterCommand),
                                _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorRegister));

  // Since duplicate key handle is found, registration process is terminated
  // before actual Register() is called on the device. Therefore, TryWink() is
  // invoked once.
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device));

  register_callback_receiver().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kUserConsentButCredentialExcluded,
            register_callback_receiver().status());
  EXPECT_FALSE(register_callback_receiver().value());
}

// Tests a scenario where one (device1) of the two devices connected has created
// a key handle provided in exclude list. We assume that duplicate key is the
// third key handle provided in the exclude list.
TEST_F(U2fRegisterTest, TestMultipleDeviceRegistrationWithDuplicateHandle) {
  // Simulate two unknown key handles followed by a duplicate key.
  auto request = CreateRegisterRequestWithRegisteredKeys(
      {u2f_parsing_utils::Materialize(test_data::kKeyHandleAlpha),
       u2f_parsing_utils::Materialize(test_data::kKeyHandleBeta),
       u2f_parsing_utils::Materialize(test_data::kKeyHandleGamma)});
  request->Start();

  // Since the first device did not create any of the key handles provided in
  // exclude list, we expect that check only sign() should be called
  // four times, and all the calls to DeviceTransact() invoke
  // MockFidoDevice::WrongData.
  auto device0 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  EXPECT_CALL(*device0.get(),
              DeviceTransactPtr(
                  u2f_parsing_utils::Materialize(
                      test_data::kU2fCheckOnlySignCommandApduWithKeyAlpha),
                  _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(
      *device0.get(),
      DeviceTransactPtr(u2f_parsing_utils::Materialize(
                            test_data::kU2fCheckOnlySignCommandApduWithKeyBeta),
                        _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(*device0.get(),
              DeviceTransactPtr(
                  u2f_parsing_utils::Materialize(
                      test_data::kU2fCheckOnlySignCommandApduWithKeyGamma),
                  _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(*device0, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device0));

  // Since the last key handle in exclude list is a duplicate key, we expect
  // that the first three calls to check only sign() invoke
  // MockFidoDevice::WrongData and that fourth sign() call invoke
  // MockFidoDevice::NoErrorSign. After duplicate key is found, process is
  // terminated after user presence is verified using bogus registration, which
  // invokes MockFidoDevice::NoErrorRegister.
  auto device1 = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(::testing::Return("device1"));
  EXPECT_CALL(*device1.get(),
              DeviceTransactPtr(
                  u2f_parsing_utils::Materialize(
                      test_data::kU2fCheckOnlySignCommandApduWithKeyAlpha),
                  _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(
      *device1.get(),
      DeviceTransactPtr(u2f_parsing_utils::Materialize(
                            test_data::kU2fCheckOnlySignCommandApduWithKeyBeta),
                        _))
      .WillOnce(::testing::Invoke(MockFidoDevice::WrongData));

  EXPECT_CALL(*device1.get(),
              DeviceTransactPtr(
                  u2f_parsing_utils::Materialize(
                      test_data::kU2fCheckOnlySignCommandApduWithKeyGamma),
                  _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorSign));

  EXPECT_CALL(*device1.get(),
              DeviceTransactPtr(u2f_parsing_utils::Materialize(
                                    test_data::kU2fFakeRegisterCommand),
                                _))
      .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorRegister));

  EXPECT_CALL(*device1, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device1));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  register_callback_receiver().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kUserConsentButCredentialExcluded,
            register_callback_receiver().status());
  EXPECT_FALSE(register_callback_receiver().value());
}

// These test the parsing of the U2F raw bytes of the registration response.
// Test that an EC public key serializes to CBOR properly.
TEST_F(U2fRegisterTest, TestSerializedPublicKey) {
  std::unique_ptr<ECPublicKey> public_key =
      ECPublicKey::ExtractFromU2fRegistrationResponse(
          u2f_parsing_utils::kEs256, GetTestRegisterResponse());
  EXPECT_EQ(GetTestECPublicKeyCOSE(), public_key->EncodeAsCOSEKey());
}

// Test that the attestation statement cbor map is constructed properly.
TEST_F(U2fRegisterTest, TestU2fAttestationStatementCBOR) {
  std::unique_ptr<FidoAttestationStatement> fido_attestation_statement =
      FidoAttestationStatement::CreateFromU2fRegisterResponse(
          GetTestRegisterResponse());
  auto cbor = cbor::CBORWriter::Write(
      cbor::CBORValue(fido_attestation_statement->GetAsCBORMap()));
  ASSERT_TRUE(cbor);
  EXPECT_EQ(GetU2fAttestationStatementCBOR(), *cbor);
}

// Tests that well-formed attested credential data serializes properly.
TEST_F(U2fRegisterTest, TestAttestedCredentialData) {
  std::unique_ptr<ECPublicKey> public_key =
      ECPublicKey::ExtractFromU2fRegistrationResponse(
          u2f_parsing_utils::kEs256, GetTestRegisterResponse());
  base::Optional<AttestedCredentialData> attested_data =
      AttestedCredentialData::CreateFromU2fRegisterResponse(
          GetTestRegisterResponse(), std::move(public_key));

  EXPECT_EQ(GetTestAttestedCredentialDataBytes(),
            attested_data->SerializeAsBytes());
}

// Tests that well-formed authenticator data serializes properly.
TEST_F(U2fRegisterTest, TestAuthenticatorData) {
  std::unique_ptr<ECPublicKey> public_key =
      ECPublicKey::ExtractFromU2fRegistrationResponse(
          u2f_parsing_utils::kEs256, GetTestRegisterResponse());
  base::Optional<AttestedCredentialData> attested_data =
      AttestedCredentialData::CreateFromU2fRegisterResponse(
          GetTestRegisterResponse(), std::move(public_key));

  constexpr uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence) |
      static_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);

  AuthenticatorData authenticator_data(
      u2f_parsing_utils::Materialize(test_data::kApplicationParameter), flags,
      std::vector<uint8_t>(4) /* counter */, std::move(attested_data));

  EXPECT_EQ(GetTestAuthenticatorDataBytes(),
            authenticator_data.SerializeToByteArray());
}

// Tests that a U2F attestation object serializes properly.
TEST_F(U2fRegisterTest, TestU2fAttestationObject) {
  std::unique_ptr<ECPublicKey> public_key =
      ECPublicKey::ExtractFromU2fRegistrationResponse(
          u2f_parsing_utils::kEs256, GetTestRegisterResponse());
  base::Optional<AttestedCredentialData> attested_data =
      AttestedCredentialData::CreateFromU2fRegisterResponse(
          GetTestRegisterResponse(), std::move(public_key));

  constexpr uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence) |
      static_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);
  AuthenticatorData authenticator_data(
      u2f_parsing_utils::Materialize(test_data::kApplicationParameter), flags,
      std::vector<uint8_t>(4) /* counter */, std::move(attested_data));

  // Construct the attestation statement.
  std::unique_ptr<FidoAttestationStatement> fido_attestation_statement =
      FidoAttestationStatement::CreateFromU2fRegisterResponse(
          GetTestRegisterResponse());

  // Construct the attestation object.
  auto attestation_object = std::make_unique<AttestationObject>(
      std::move(authenticator_data), std::move(fido_attestation_statement));

  EXPECT_EQ(GetTestAttestationObjectBytes(),
            attestation_object->SerializeToCBOREncodedBytes());
}

// Test that a U2F register response is properly parsed.
TEST_F(U2fRegisterTest, TestRegisterResponseData) {
  base::Optional<AuthenticatorMakeCredentialResponse> response =
      AuthenticatorMakeCredentialResponse::CreateFromU2fRegisterResponse(
          u2f_parsing_utils::Materialize(test_data::kApplicationParameter),
          GetTestRegisterResponse());
  EXPECT_EQ(GetTestCredentialRawIdBytes(), response->raw_credential_id());
  EXPECT_EQ(GetTestAttestationObjectBytes(),
            response->GetCBOREncodedAttestationObject());
}

MATCHER_P(IndicatesIndividualAttestation, expected, "") {
  return arg.size() > 2 && ((arg[2] & 0x80) == 0x80) == expected;
}

TEST_F(U2fRegisterTest, TestIndividualAttestation) {
  // Test that the individual attestation flag is correctly reflected in the
  // resulting registration APDU.
  for (const auto& individual_attestation : {false, true}) {
    SCOPED_TRACE(individual_attestation);

    ForgeNextHidDiscovery();
    TestRegisterCallback cb;
    auto request = std::make_unique<U2fRegister>(
        nullptr /* connector */,
        base::flat_set<FidoTransportProtocol>(
            {FidoTransportProtocol::kUsbHumanInterfaceDevice}) /* transports */,
        key_handles_, challenge_parameter_, application_parameter_,
        individual_attestation, cb.callback());
    request->Start();
    discovery()->WaitForCallToStartAndSimulateSuccess();

    auto device = std::make_unique<MockFidoDevice>();
    EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
    EXPECT_CALL(*device,
                DeviceTransactPtr(
                    IndicatesIndividualAttestation(individual_attestation), _))
        .WillOnce(::testing::Invoke(MockFidoDevice::NoErrorRegister));
    EXPECT_CALL(*device, TryWinkRef(_))
        .WillOnce(::testing::Invoke(MockFidoDevice::WinkDoNothing));
    discovery()->AddDevice(std::move(device));

    cb.WaitForCallback();
    EXPECT_EQ(FidoReturnCode::kSuccess, cb.status());
    ASSERT_TRUE(cb.value());
    EXPECT_EQ(GetTestCredentialRawIdBytes(), cb.value()->raw_credential_id());
  }
}

}  // namespace device
