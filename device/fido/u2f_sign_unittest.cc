// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_sign.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fake_u2f_discovery.h"
#include "device/fido/mock_u2f_device.h"
#include "device/fido/sign_response_data.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/u2f_response_test_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {

namespace {

constexpr uint8_t kAppId[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                              0x08, 0x09, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                              0x06, 0x07, 0x08, 0x09, 0x00, 0x01, 0x02, 0x03,
                              0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x01};

constexpr uint8_t kChallengeDigest[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x01,
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x01};

constexpr uint8_t kKeyHandle[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x01,
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x01};

constexpr uint8_t kTestRelyingPartyIdSHA256[32] = {
    0xd4, 0xc9, 0xd9, 0x02, 0x73, 0x26, 0x27, 0x1a, 0x89, 0xce, 0x51,
    0xfc, 0xaf, 0x32, 0x8e, 0xd6, 0x73, 0xf1, 0x7b, 0xe3, 0x34, 0x69,
    0xff, 0x97, 0x9e, 0x8a, 0xb8, 0xdd, 0x50, 0x1e, 0x66, 0x4f,
};

// Signature counter returned within the authenticator data.
constexpr uint8_t kTestSignatureCounter[] = {0x00, 0x00, 0x00, 0x25};

// Test data specific to GetAssertion/Sign.
constexpr uint8_t kTestAssertionSignature[] = {
    0x30, 0x45, 0x02, 0x21, 0x00, 0xCA, 0xA5, 0x3E, 0x91, 0x0D, 0xB7, 0x5E,
    0xDE, 0xAF, 0x72, 0xCF, 0x9F, 0x6F, 0x54, 0xE5, 0x20, 0x5B, 0xBB, 0xB9,
    0x2F, 0x0B, 0x9F, 0x7D, 0xC6, 0xF8, 0xD4, 0x7B, 0x19, 0x70, 0xED, 0xFE,
    0xBC, 0x02, 0x20, 0x06, 0x32, 0x83, 0x65, 0x26, 0x4E, 0xBE, 0xFE, 0x35,
    0x3C, 0x95, 0x91, 0xDF, 0xCE, 0x7D, 0x73, 0x15, 0x98, 0x64, 0xDF, 0xEA,
    0xB7, 0x87, 0xF1, 0x5D, 0xF8, 0xA5, 0x97, 0xD0, 0x85, 0x0C, 0xA2};

// The authenticator data for sign responses.
constexpr uint8_t kTestSignAuthenticatorData[] = {
    // clang-format off
    // sha256 hash of kTestRelyingPartyId
    0xD4, 0xC9, 0xD9, 0x02, 0x73, 0x26, 0x27, 0x1A, 0x89, 0xCE, 0x51,
    0xFC, 0xAF, 0x32, 0x8E, 0xD6, 0x73, 0xF1, 0x7B, 0xE3, 0x34, 0x69,
    0xFF, 0x97, 0x9E, 0x8A, 0xB8, 0xDD, 0x50, 0x1E, 0x66, 0x4F,
    0x01,  // flags (TUP bit set)
    0x00, 0x00, 0x00, 0x25  // counter
    // clang-format on
};

std::vector<uint8_t> GetTestCredentialRawIdBytes() {
  return std::vector<uint8_t>(std::begin(test_data::kTestCredentialRawIdBytes),
                              std::end(test_data::kTestCredentialRawIdBytes));
}

std::vector<uint8_t> GetTestSignResponse() {
  return std::vector<uint8_t>(std::begin(test_data::kTestU2fSignResponse),
                              std::end(test_data::kTestU2fSignResponse));
}

std::vector<uint8_t> GetTestAuthenticatorData() {
  return std::vector<uint8_t>(std::begin(kTestSignAuthenticatorData),
                              std::end(kTestSignAuthenticatorData));
}

std::vector<uint8_t> GetTestAssertionSignature() {
  return std::vector<uint8_t>(std::begin(kTestAssertionSignature),
                              std::end(kTestAssertionSignature));
}

std::vector<uint8_t> GetTestSignatureCounter() {
  return std::vector<uint8_t>(std::begin(kTestSignatureCounter),
                              std::end(kTestSignatureCounter));
}

// Get a subset of the response for testing error handling.
std::vector<uint8_t> GetTestCorruptedSignResponse(size_t length) {
  DCHECK_LE(length, arraysize(test_data::kTestU2fSignResponse));
  return std::vector<uint8_t>(test_data::kTestU2fSignResponse,
                              test_data::kTestU2fSignResponse + length);
}

using TestSignCallback = ::device::test::StatusAndValueCallbackReceiver<
    U2fReturnCode,
    base::Optional<SignResponseData>>;

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
    return CreateSignRequestWithKeys(std::vector<std::vector<uint8_t>>());
  }

  std::unique_ptr<U2fSign> CreateSignRequestWithKeys(
      std::vector<std::vector<uint8_t>> registered_keys) {
    ForgeNextHidDiscovery();
    return std::make_unique<U2fSign>(
        nullptr /* connector */,
        base::flat_set<U2fTransportProtocol>(
            {U2fTransportProtocol::kUsbHumanInterfaceDevice}),
        registered_keys, std::vector<uint8_t>(32), std::vector<uint8_t>(32),
        base::nullopt, sign_callback_receiver_.callback());
  }

  test::FakeU2fDiscovery* discovery() const { return discovery_; }
  TestSignCallback& sign_callback_receiver() { return sign_callback_receiver_; }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  test::ScopedFakeU2fDiscoveryFactory scoped_fake_discovery_factory_;
  test::FakeU2fDiscovery* discovery_;
  TestSignCallback sign_callback_receiver_;
};

TEST_F(U2fSignTest, TestCreateSignApduCommand) {
  constexpr uint8_t kSignApduEncoded[] = {
      // clang-format off
      0x00, 0x02, 0x03, 0x00,  // CLA, INS, P1, P2 APDU instruction parameters
      0x00, 0x00, 0x61,       // Data Length (3 bytes in big endian order)
      // Application parameter
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      0x00, 0x01,
      // Challenge parameter
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      0x00, 0x01,
      0x20,  // Key handle length
      // Key Handle
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00,
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x01,
      0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x01
      // clang-format on
  };

  constexpr uint8_t kSignApduEncodedCheckOnly[] = {
      // clang-format off
      0x00, 0x02, 0x07, 0x00,  // CLA, INS, P1, P2 APDU instruction parameters
      0x00, 0x00, 0x61,       // Data Length (3 bytes in big endian order)
      // Application parameter
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      0x00, 0x01,
      // Challenge parameter
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      0x00, 0x01,
      0x20,  // Key handle length
      // Key Handle
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00,
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x01,
      0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x01
      // clang-format on
  };

  const std::vector<uint8_t> key_handle(std::begin(kKeyHandle),
                                        std::end(kKeyHandle));
  U2fSign u2f_sign(nullptr /* connector */, {} /* transports */,
                   {key_handle} /* registered_keys */,
                   std::vector<uint8_t>(std::begin(kChallengeDigest),
                                        std::end(kChallengeDigest)),
                   std::vector<uint8_t>(std::begin(kAppId), std::end(kAppId)),
                   base::nullopt, sign_callback_receiver().callback());

  const auto encoded_sign = u2f_sign.GetU2fSignApduCommand(key_handle);
  ASSERT_TRUE(encoded_sign);
  EXPECT_THAT(encoded_sign->GetEncodedCommand(),
              ::testing::ElementsAreArray(kSignApduEncoded));

  const auto encoded_sign_check_only =
      u2f_sign.GetU2fSignApduCommand(key_handle, true);
  ASSERT_TRUE(encoded_sign_check_only);
  EXPECT_THAT(encoded_sign_check_only->GetEncodedCommand(),
              ::testing::ElementsAreArray(kSignApduEncodedCheckOnly));
}

TEST_F(U2fSignTest, TestSignSuccess) {
  const std::vector<uint8_t> signing_key_handle(32, 0x0A);
  auto request = CreateSignRequestWithKeys({signing_key_handle});
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  EXPECT_CALL(*device, DeviceTransactPtr(_, _))
      .WillOnce(::testing::Invoke(MockU2fDevice::NoErrorSign));
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockU2fDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device));

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, sign_callback_receiver().status());

  // Correct key was sent so a sign response is expected.
  EXPECT_EQ(GetTestAssertionSignature(),
            sign_callback_receiver().value()->signature());

  // Verify that we get the key handle used for signing.
  EXPECT_EQ(signing_key_handle, sign_callback_receiver().value()->raw_id());
}

TEST_F(U2fSignTest, TestDelayedSuccess) {
  const std::vector<uint8_t> signing_key_handle(32, 0x0A);
  auto request = CreateSignRequestWithKeys({signing_key_handle});
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  // Go through the state machine twice before success.
  auto device = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  EXPECT_CALL(*device, DeviceTransactPtr(_, _))
      .WillOnce(::testing::Invoke(MockU2fDevice::NotSatisfied))
      .WillOnce(::testing::Invoke(MockU2fDevice::NoErrorSign));
  EXPECT_CALL(*device, TryWinkRef(_))
      .Times(2)
      .WillRepeatedly(::testing::Invoke(MockU2fDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device));

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, sign_callback_receiver().status());

  // Correct key was sent so a sign response is expected
  EXPECT_EQ(GetTestAssertionSignature(),
            sign_callback_receiver().value()->signature());

  // Verify that we get the key handle used for signing
  EXPECT_EQ(signing_key_handle, sign_callback_receiver().value()->raw_id());
}

TEST_F(U2fSignTest, TestMultipleHandles) {
  // Three wrong keys followed by a correct key ensuring the wrong keys will be
  // tested first.
  const std::vector<uint8_t> correct_key_handle(32, 0x0A);
  auto request = CreateSignRequestWithKeys(
      {std::vector<uint8_t>(32, 0x0B), std::vector<uint8_t>(32, 0x0C),
       std::vector<uint8_t>(32, 0x0D), correct_key_handle});
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockU2fDevice>();
  // Wrong key would respond with SW_WRONG_DATA.
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  EXPECT_CALL(*device, DeviceTransactPtr(_, _))
      .WillOnce(::testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(::testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(::testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(::testing::Invoke(MockU2fDevice::NoErrorSign));
  // Only one wink expected per device.
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockU2fDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device));

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, sign_callback_receiver().status());

  // Correct key was sent so a sign response is expected.
  EXPECT_EQ(GetTestAssertionSignature(),
            sign_callback_receiver().value()->signature());

  // Verify that we get the key handle used for signing.
  EXPECT_EQ(correct_key_handle, sign_callback_receiver().value()->raw_id());
}

TEST_F(U2fSignTest, TestMultipleDevices) {
  const std::vector<uint8_t> correct_key_handle(32, 0x0A);
  auto request = CreateSignRequestWithKeys(
      {correct_key_handle, std::vector<uint8_t>(32, 0x0B)});
  request->Start();

  auto device0 = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(::testing::Return("device0"));
  EXPECT_CALL(*device0, DeviceTransactPtr(_, _))
      .WillOnce(::testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(::testing::Invoke(MockU2fDevice::NotSatisfied));
  // One wink per device.
  EXPECT_CALL(*device0, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockU2fDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device0));

  // Second device will have a successful touch.
  auto device1 = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(::testing::Return("device1"));
  EXPECT_CALL(*device1, DeviceTransactPtr(_, _))
      .WillOnce(::testing::Invoke(MockU2fDevice::NoErrorSign));
  // One wink per device.
  EXPECT_CALL(*device1, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockU2fDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device1));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, sign_callback_receiver().status());

  // Correct key was sent so a sign response is expected.
  EXPECT_EQ(GetTestAssertionSignature(),
            sign_callback_receiver().value()->signature());

  // Verify that we get the key handle used for signing.
  EXPECT_EQ(correct_key_handle, sign_callback_receiver().value()->raw_id());
}

TEST_F(U2fSignTest, TestFakeEnroll) {
  const std::vector<uint8_t> correct_key_handle(32, 0x0A);
  auto request = CreateSignRequestWithKeys(
      {correct_key_handle, std::vector<uint8_t>(32, 0x0B)});
  request->Start();

  auto device0 = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(::testing::Return("device0"));
  EXPECT_CALL(*device0, DeviceTransactPtr(_, _))
      .WillOnce(::testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(::testing::Invoke(MockU2fDevice::NotSatisfied));
  // One wink per device.
  EXPECT_CALL(*device0, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockU2fDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device0));

  // Second device will be have a successful touch.
  auto device1 = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(::testing::Return("device1"));
  // Both keys will be tried, when both fail, register is tried on that device.
  EXPECT_CALL(*device1, DeviceTransactPtr(_, _))
      .WillOnce(::testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(::testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(::testing::Invoke(MockU2fDevice::NoErrorRegister));
  // One wink per device.
  EXPECT_CALL(*device1, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockU2fDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device1));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  sign_callback_receiver().WaitForCallback();
  // Device that responded had no correct keys.
  EXPECT_EQ(U2fReturnCode::CONDITIONS_NOT_SATISFIED,
            sign_callback_receiver().status());
  EXPECT_FALSE(sign_callback_receiver().value());
}

TEST_F(U2fSignTest, TestAuthenticatorDataForSign) {
  constexpr uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence);

  EXPECT_EQ(
      GetTestAuthenticatorData(),
      AuthenticatorData(std::vector<uint8_t>(kTestRelyingPartyIdSHA256,
                                             kTestRelyingPartyIdSHA256 + 32),
                        flags, GetTestSignatureCounter(), base::nullopt)
          .SerializeToByteArray());
}

TEST_F(U2fSignTest, TestSignResponseData) {
  base::Optional<SignResponseData> response =
      SignResponseData::CreateFromU2fSignResponse(
          std::vector<uint8_t>(kTestRelyingPartyIdSHA256,
                               kTestRelyingPartyIdSHA256 + 32),
          GetTestSignResponse(), GetTestCredentialRawIdBytes());
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(GetTestCredentialRawIdBytes(), response->raw_id());
  EXPECT_EQ(GetTestAuthenticatorData(), response->GetAuthenticatorDataBytes());
  EXPECT_EQ(GetTestAssertionSignature(), response->signature());
}

TEST_F(U2fSignTest, TestNullKeyHandle) {
  base::Optional<SignResponseData> response =
      SignResponseData::CreateFromU2fSignResponse(
          std::vector<uint8_t>(kTestRelyingPartyIdSHA256,
                               kTestRelyingPartyIdSHA256 + 32),
          GetTestSignResponse(), std::vector<uint8_t>());
  EXPECT_FALSE(response);
}

TEST_F(U2fSignTest, TestNullResponse) {
  base::Optional<SignResponseData> response =
      SignResponseData::CreateFromU2fSignResponse(
          std::vector<uint8_t>(kTestRelyingPartyIdSHA256,
                               kTestRelyingPartyIdSHA256 + 32),
          std::vector<uint8_t>(), GetTestCredentialRawIdBytes());
  EXPECT_FALSE(response);
}

TEST_F(U2fSignTest, TestCorruptedCounter) {
  // A sign response of less than 5 bytes.
  base::Optional<SignResponseData> response =
      SignResponseData::CreateFromU2fSignResponse(
          std::vector<uint8_t>(kTestRelyingPartyIdSHA256,
                               kTestRelyingPartyIdSHA256 + 32),
          GetTestCorruptedSignResponse(3), GetTestCredentialRawIdBytes());
  EXPECT_FALSE(response);
}

TEST_F(U2fSignTest, TestCorruptedSignature) {
  // A sign response no more than 5 bytes.
  base::Optional<SignResponseData> response =
      SignResponseData::CreateFromU2fSignResponse(
          std::vector<uint8_t>(kTestRelyingPartyIdSHA256,
                               kTestRelyingPartyIdSHA256 + 32),
          GetTestCorruptedSignResponse(5), GetTestCredentialRawIdBytes());
  EXPECT_FALSE(response);
}

// Device returns success, but the response is unparse-able.
TEST_F(U2fSignTest, TestSignWithCorruptedResponse) {
  auto request = CreateSignRequestWithKeys({std::vector<uint8_t>(32, 0x0A)});
  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  EXPECT_CALL(*device, DeviceTransactPtr(_, _))
      .WillOnce(::testing::Invoke(MockU2fDevice::SignWithCorruptedResponse));
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockU2fDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device));

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(U2fReturnCode::FAILURE, sign_callback_receiver().status());
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
      base::flat_set<U2fTransportProtocol>(
          {U2fTransportProtocol::kUsbHumanInterfaceDevice}),
      std::vector<std::vector<uint8_t>>({signing_key_handle}),
      std::vector<uint8_t>(32), primary_app_param, alt_app_param,
      sign_callback_receiver_.callback());

  request->Start();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockU2fDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  // The first request will use the primary app_param, which will be rejected.
  EXPECT_CALL(*device,
              DeviceTransactPtr(WithApplicationParameter(primary_app_param), _))
      .WillOnce(::testing::Invoke(MockU2fDevice::WrongData));
  // After the rejection, the alternative should be tried.
  EXPECT_CALL(*device,
              DeviceTransactPtr(WithApplicationParameter(alt_app_param), _))
      .WillOnce(::testing::Invoke(MockU2fDevice::NoErrorSign));
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(::testing::Invoke(MockU2fDevice::WinkDoNothing));
  discovery()->AddDevice(std::move(device));

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, sign_callback_receiver().status());

  EXPECT_EQ(GetTestAssertionSignature(),
            sign_callback_receiver().value()->signature());
  EXPECT_EQ(signing_key_handle, sign_callback_receiver().value()->raw_id());
}

}  // namespace device
