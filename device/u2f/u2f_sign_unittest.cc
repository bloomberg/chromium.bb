// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_sign.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "device/u2f/authenticator_data.h"
#include "device/u2f/mock_u2f_device.h"
#include "device/u2f/mock_u2f_discovery.h"
#include "device/u2f/sign_response_data.h"
#include "device/u2f/u2f_response_test_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {

namespace {

constexpr char kTestRelyingPartyId[] = "google.com";

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

}  // namespace

class U2fSignTest : public testing::Test {
 public:
  U2fSignTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

class TestSignCallback {
 public:
  TestSignCallback()
      : callback_(base::Bind(&TestSignCallback::ReceivedCallback,
                             base::Unretained(this))) {}
  ~TestSignCallback() = default;

  void ReceivedCallback(U2fReturnCode status_code,
                        base::Optional<SignResponseData> response_data) {
    response_ = std::make_pair(status_code, std::move(response_data));
    closure_.Run();
  }

  void WaitForCallback() {
    closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
  }

  U2fReturnCode GetReturnCode() { return std::get<0>(response_); }

  const base::Optional<SignResponseData>& GetResponseData() {
    return std::get<1>(response_);
  }

  U2fSign::SignResponseCallback& callback() { return callback_; }

 private:
  std::pair<U2fReturnCode, base::Optional<SignResponseData>> response_;
  base::Closure closure_;
  U2fSign::SignResponseCallback callback_;
  base::RunLoop run_loop_;
};

TEST_F(U2fSignTest, TestSignSuccess) {
  auto device = std::make_unique<MockU2fDevice>();
  MockU2fDiscovery discovery;

  EXPECT_CALL(*device, GetId()).WillOnce(testing::Return("device"));
  EXPECT_CALL(*device, DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign));
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(discovery, Start())
      .WillOnce(
          testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccessAsync));
  TestSignCallback cb;

  std::vector<uint8_t> key(32, 0xA);
  std::unique_ptr<U2fRequest> request = U2fSign::TrySign(
      kTestRelyingPartyId, {&discovery},
      std::vector<std::vector<uint8_t>>({key}), std::vector<uint8_t>(32),
      std::vector<uint8_t>(32), std::move(cb.callback()));
  discovery.AddDevice(std::move(device));
  cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, cb.GetReturnCode());

  // Correct key was sent so a sign response is expected
  EXPECT_EQ(GetTestAssertionSignature(), cb.GetResponseData()->signature());

  // Verify that we get the key handle used for signing
  EXPECT_EQ(key, cb.GetResponseData()->raw_id());
}

TEST_F(U2fSignTest, TestDelayedSuccess) {
  auto device = std::make_unique<MockU2fDevice>();
  MockU2fDiscovery discovery;

  // Go through the state machine twice before success
  EXPECT_CALL(*device, GetId()).WillOnce(testing::Return("device"));
  EXPECT_CALL(*device, DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign));
  EXPECT_CALL(*device, TryWinkRef(_))
      .Times(2)
      .WillRepeatedly(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(discovery, Start())
      .WillOnce(
          testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccessAsync));
  TestSignCallback cb;

  std::vector<uint8_t> key(32, 0xA);
  std::unique_ptr<U2fRequest> request = U2fSign::TrySign(
      kTestRelyingPartyId, {&discovery},
      std::vector<std::vector<uint8_t>>({key}), std::vector<uint8_t>(32),
      std::vector<uint8_t>(32), std::move(cb.callback()));
  request->Start();
  discovery.AddDevice(std::move(device));

  cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, cb.GetReturnCode());

  // Correct key was sent so a sign response is expected
  EXPECT_EQ(GetTestAssertionSignature(), cb.GetResponseData()->signature());

  // Verify that we get the key handle used for signing
  EXPECT_EQ(key, cb.GetResponseData()->raw_id());
}

TEST_F(U2fSignTest, TestMultipleHandles) {
  std::vector<uint8_t> key(32, 0xA);
  std::vector<uint8_t> wrong_key0(32, 0xB);
  std::vector<uint8_t> wrong_key1(32, 0xC);
  std::vector<uint8_t> wrong_key2(32, 0xD);
  // Put wrong key first to ensure that it will be tested before the correct key
  std::vector<std::vector<uint8_t>> handles = {wrong_key0, wrong_key1,
                                               wrong_key2, key};
  auto device = std::make_unique<MockU2fDevice>();
  MockU2fDiscovery discovery;

  // Wrong key would respond with SW_WRONG_DATA
  EXPECT_CALL(*device, GetId()).WillOnce(testing::Return("device"));
  EXPECT_CALL(*device, DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign));
  // Only one wink expected per device
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(discovery, Start())
      .WillOnce(
          testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccessAsync));

  TestSignCallback cb;
  std::unique_ptr<U2fRequest> request = U2fSign::TrySign(
      kTestRelyingPartyId, {&discovery}, handles, std::vector<uint8_t>(32),
      std::vector<uint8_t>(32), std::move(cb.callback()));
  discovery.AddDevice(std::move(device));

  cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, cb.GetReturnCode());

  // Correct key was sent so a sign response is expected
  EXPECT_EQ(GetTestAssertionSignature(), cb.GetResponseData()->signature());

  // Verify that we get the key handle used for signing
  EXPECT_EQ(key, cb.GetResponseData()->raw_id());
}

TEST_F(U2fSignTest, TestMultipleDevices) {
  std::vector<uint8_t> key0(32, 0xA);
  std::vector<uint8_t> key1(32, 0xB);
  // Second device will have a successful touch
  std::vector<std::vector<uint8_t>> handles = {key0, key1};
  auto device0 = std::make_unique<MockU2fDevice>();
  auto device1 = std::make_unique<MockU2fDevice>();
  MockU2fDiscovery discovery;

  EXPECT_CALL(*device0, GetId()).WillOnce(testing::Return("device0"));
  EXPECT_CALL(*device1, GetId()).WillOnce(testing::Return("device1"));

  EXPECT_CALL(*device0, DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied));
  // One wink per device
  EXPECT_CALL(*device0, TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(*device1, DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorSign));
  EXPECT_CALL(*device1, TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(discovery, Start())
      .WillOnce(
          testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccessAsync));

  TestSignCallback cb;
  std::unique_ptr<U2fRequest> request = U2fSign::TrySign(
      kTestRelyingPartyId, {&discovery}, handles, std::vector<uint8_t>(32),
      std::vector<uint8_t>(32), std::move(cb.callback()));
  discovery.AddDevice(std::move(device0));
  discovery.AddDevice(std::move(device1));

  cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::SUCCESS, cb.GetReturnCode());

  // Correct key was sent so a sign response is expected
  EXPECT_EQ(GetTestAssertionSignature(), cb.GetResponseData()->signature());

  // Verify that we get the key handle used for signing
  EXPECT_EQ(key0, cb.GetResponseData()->raw_id());
}

TEST_F(U2fSignTest, TestFakeEnroll) {
  std::vector<uint8_t> key0(32, 0xA);
  std::vector<uint8_t> key1(32, 0xB);
  // Second device will be have a successful touch
  std::vector<std::vector<uint8_t>> handles = {key0, key1};
  auto device0 = std::make_unique<MockU2fDevice>();
  auto device1 = std::make_unique<MockU2fDevice>();
  MockU2fDiscovery discovery;

  EXPECT_CALL(*device0, GetId()).WillOnce(testing::Return("device0"));
  EXPECT_CALL(*device1, GetId()).WillOnce(testing::Return("device1"));
  EXPECT_CALL(*device0, DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NotSatisfied));
  // One wink per device
  EXPECT_CALL(*device0, TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  // Both keys will be tried, when both fail, register is tried on that device
  EXPECT_CALL(*device1, DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::WrongData))
      .WillOnce(testing::Invoke(MockU2fDevice::NoErrorRegister));
  EXPECT_CALL(*device1, TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(discovery, Start())
      .WillOnce(
          testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccessAsync));

  TestSignCallback cb;
  std::unique_ptr<U2fRequest> request = U2fSign::TrySign(
      kTestRelyingPartyId, {&discovery}, handles, std::vector<uint8_t>(32),
      std::vector<uint8_t>(32), std::move(cb.callback()));
  request->Start();
  discovery.AddDevice(std::move(device0));
  discovery.AddDevice(std::move(device1));

  cb.WaitForCallback();

  // Device that responded had no correct keys.
  EXPECT_EQ(U2fReturnCode::CONDITIONS_NOT_SATISFIED, cb.GetReturnCode());
  EXPECT_FALSE(cb.GetResponseData());
}

TEST_F(U2fSignTest, TestAuthenticatorDataForSign) {
  constexpr uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence);

  EXPECT_EQ(GetTestAuthenticatorData(),
            AuthenticatorData(kTestRelyingPartyId, flags,
                              GetTestSignatureCounter(), base::nullopt)
                .SerializeToByteArray());
}

TEST_F(U2fSignTest, TestSignResponseData) {
  base::Optional<SignResponseData> response =
      SignResponseData::CreateFromU2fSignResponse(
          kTestRelyingPartyId, GetTestSignResponse(),
          GetTestCredentialRawIdBytes());
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(GetTestCredentialRawIdBytes(), response->raw_id());
  EXPECT_EQ(GetTestAuthenticatorData(), response->GetAuthenticatorDataBytes());
  EXPECT_EQ(GetTestAssertionSignature(), response->signature());
}

TEST_F(U2fSignTest, TestNullKeyHandle) {
  base::Optional<SignResponseData> response =
      SignResponseData::CreateFromU2fSignResponse(
          kTestRelyingPartyId, GetTestSignResponse(), std::vector<uint8_t>());
  EXPECT_FALSE(response);
}

TEST_F(U2fSignTest, TestNullResponse) {
  base::Optional<SignResponseData> response =
      SignResponseData::CreateFromU2fSignResponse(
          kTestRelyingPartyId, std::vector<uint8_t>(),
          GetTestCredentialRawIdBytes());
  EXPECT_FALSE(response);
}

TEST_F(U2fSignTest, TestCorruptedCounter) {
  // A sign response of less than 5 bytes.
  base::Optional<SignResponseData> response =
      SignResponseData::CreateFromU2fSignResponse(
          kTestRelyingPartyId, GetTestCorruptedSignResponse(3),
          GetTestCredentialRawIdBytes());
  EXPECT_FALSE(response);
}

TEST_F(U2fSignTest, TestCorruptedSignature) {
  // A sign response no more than 5 bytes.
  base::Optional<SignResponseData> response =
      SignResponseData::CreateFromU2fSignResponse(
          kTestRelyingPartyId, GetTestCorruptedSignResponse(5),
          GetTestCredentialRawIdBytes());
  EXPECT_FALSE(response);
}

// Device returns success, but the response is unparse-able.
TEST_F(U2fSignTest, TestSignWithCorruptedResponse) {
  auto device = std::make_unique<MockU2fDevice>();
  MockU2fDiscovery discovery;

  EXPECT_CALL(*device, GetId()).WillOnce(testing::Return("device"));
  EXPECT_CALL(*device, DeviceTransactPtr(_, _))
      .WillOnce(testing::Invoke(MockU2fDevice::SignWithCorruptedResponse));
  EXPECT_CALL(*device, TryWinkRef(_))
      .WillOnce(testing::Invoke(MockU2fDevice::WinkDoNothing));
  EXPECT_CALL(discovery, Start())
      .WillOnce(
          testing::Invoke(&discovery, &MockU2fDiscovery::StartSuccessAsync));
  TestSignCallback cb;

  std::vector<uint8_t> key(32, 0xA);
  std::unique_ptr<U2fRequest> request = U2fSign::TrySign(
      kTestRelyingPartyId, {&discovery},
      std::vector<std::vector<uint8_t>>({key}), std::vector<uint8_t>(32),
      std::vector<uint8_t>(32), std::move(cb.callback()));
  discovery.AddDevice(std::move(device));
  cb.WaitForCallback();
  EXPECT_EQ(U2fReturnCode::FAILURE, cb.GetReturnCode());
  EXPECT_FALSE(cb.GetResponseData());
}
}  // namespace device
