// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/attestation/platform_verification_flow.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace chromeos {
namespace attestation {

namespace {

const char kTestID[] = "test_id";
const char kTestChallenge[] = "test_challenge";
const char kTestResponse[] = "test_challenge_response";
const char kTestCertificate[] = "test_certificate";
const char kTestEmail[] = "test_email@chromium.org";

class FakeDelegate : public PlatformVerificationFlow::Delegate {
 public:
  FakeDelegate() : response_(PlatformVerificationFlow::CONSENT_RESPONSE_ALLOW),
                   num_consent_calls_(0),
                   attestation_disabled_(false),
                   origin_consent_required_(false),
                   always_ask_required_(false),
                   update_settings_result_(true) {}
  virtual ~FakeDelegate() {}

  virtual void ShowConsentPrompt(
      PlatformVerificationFlow::ConsentType type,
      content::WebContents* web_contents,
      const PlatformVerificationFlow::Delegate::ConsentCallback& callback)
      OVERRIDE {
    num_consent_calls_++;
    callback.Run(response_);
  }

  virtual bool IsAttestationDisabled() OVERRIDE {
    return attestation_disabled_;
  }

  virtual bool IsOriginConsentRequired(
      content::WebContents* web_contents) OVERRIDE {
    return origin_consent_required_;
  }

  virtual bool IsAlwaysAskRequired(
      content::WebContents* web_contents) OVERRIDE {
    return always_ask_required_;
  }

  virtual bool UpdateSettings(
      content::WebContents* web_contents,
      PlatformVerificationFlow::ConsentType consent_type,
      PlatformVerificationFlow::ConsentResponse consent_response) OVERRIDE {
    return update_settings_result_;
  }

  void set_response(PlatformVerificationFlow::ConsentResponse response) {
    response_ = response;
  }

  int num_consent_calls() {
    return num_consent_calls_;
  }

  void set_attestation_disabled(bool attestation_disabled) {
    attestation_disabled_ = attestation_disabled;
  }

  void set_origin_consent_required(bool origin_consent_required) {
    origin_consent_required_ = origin_consent_required;
  }

  void set_always_ask_required(bool always_ask_required) {
    always_ask_required_ = always_ask_required;
  }

  void set_update_settings_result(bool update_settings_result) {
    update_settings_result_ = update_settings_result;
  }

 private:
  PlatformVerificationFlow::ConsentResponse response_;
  int num_consent_calls_;
  bool attestation_disabled_;
  bool origin_consent_required_;
  bool always_ask_required_;
  bool update_settings_result_;


  DISALLOW_COPY_AND_ASSIGN(FakeDelegate);
};

class CustomFakeCryptohomeClient : public FakeCryptohomeClient {
 public:
  CustomFakeCryptohomeClient() : call_status_(DBUS_METHOD_CALL_SUCCESS),
                                 attestation_enrolled_(true) {}
  virtual void TpmAttestationIsEnrolled(
      const BoolDBusMethodCallback& callback) OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback,
                                                      call_status_,
                                                      attestation_enrolled_));
  }

  void set_call_status(DBusMethodCallStatus call_status) {
    call_status_ = call_status;
  }

  void set_attestation_enrolled(bool attestation_enrolled) {
    attestation_enrolled_ = attestation_enrolled;
  }

 private:
  DBusMethodCallStatus call_status_;
  bool attestation_enrolled_;
};

}  // namespace

class PlatformVerificationFlowTest : public ::testing::Test {
 public:
  PlatformVerificationFlowTest()
      : message_loop_(base::MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        certificate_success_(true),
        sign_challenge_success_(true),
        result_(PlatformVerificationFlow::INTERNAL_ERROR) {}

  void SetUp() {
    // Configure a user for the mock user manager.
    mock_user_manager_.SetActiveUser(kTestEmail);

    // Create a verifier for tests to call.
    verifier_.reset(new PlatformVerificationFlow(&mock_attestation_flow_,
                                                 &mock_async_caller_,
                                                 &fake_cryptohome_client_,
                                                 &mock_user_manager_,
                                                 &fake_delegate_));

    // Create a callback for tests to use with verifier_.
    callback_ = base::Bind(&PlatformVerificationFlowTest::FakeChallengeCallback,
                           base::Unretained(this));
  }

  void TearDown() {
    verifier_.reset();
  }

  void ExpectAttestationFlow() {
    // When consent is not given or the feature is disabled, it is important
    // that there are no calls to the attestation service.  Thus, a test must
    // explicitly expect these calls or the mocks will fail the test.

    // Configure the mock AttestationFlow to call FakeGetCertificate.
    EXPECT_CALL(mock_attestation_flow_,
                GetCertificate(PROFILE_CONTENT_PROTECTION_CERTIFICATE,
                               kTestEmail, kTestID, _, _))
        .WillRepeatedly(WithArgs<4>(Invoke(
            this, &PlatformVerificationFlowTest::FakeGetCertificate)));

    // Configure the mock AsyncMethodCaller to call FakeSignChallenge.
    std::string expected_key_name = std::string(kContentProtectionKeyPrefix) +
                                    std::string(kTestID);
    EXPECT_CALL(mock_async_caller_,
                TpmAttestationSignSimpleChallenge(KEY_USER, expected_key_name,
                                                  kTestChallenge, _))
        .WillRepeatedly(WithArgs<3>(Invoke(
            this, &PlatformVerificationFlowTest::FakeSignChallenge)));
  }

  void FakeGetCertificate(
      const AttestationFlow::CertificateCallback& callback) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback,
                                                      certificate_success_,
                                                      kTestCertificate));
  }

  void FakeSignChallenge(
      const cryptohome::AsyncMethodCaller::DataCallback& callback) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback,
                                                      sign_challenge_success_,
                                                      kTestResponse));
  }

  void FakeChallengeCallback(PlatformVerificationFlow::Result result,
                             const std::string& response,
                             const std::string& certificate) {
    result_ = result;
    challenge_response_ = response;
    certificate_ = certificate;
  }

 protected:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  StrictMock<MockAttestationFlow> mock_attestation_flow_;
  cryptohome::MockAsyncMethodCaller mock_async_caller_;
  CustomFakeCryptohomeClient fake_cryptohome_client_;
  MockUserManager mock_user_manager_;
  FakeDelegate fake_delegate_;
  scoped_ptr<PlatformVerificationFlow> verifier_;

  // Controls result of FakeGetCertificate.
  bool certificate_success_;

  // Controls result of FakeSignChallenge.
  bool sign_challenge_success_;

  // Callback function and data.
  PlatformVerificationFlow::ChallengeCallback callback_;
  PlatformVerificationFlow::Result result_;
  std::string challenge_response_;
  std::string certificate_;
};

TEST_F(PlatformVerificationFlowTest, SuccessNoConsent) {
  // Make sure the call will fail if consent is requested.
  fake_delegate_.set_response(PlatformVerificationFlow::CONSENT_RESPONSE_DENY);
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(kTestResponse, challenge_response_);
  EXPECT_EQ(kTestCertificate, certificate_);
  EXPECT_EQ(0, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, SuccessWithOriginConsent) {
  // Enable two conditions and make sure consent is not requested twice.
  fake_delegate_.set_origin_consent_required(true);
  fake_delegate_.set_always_ask_required(true);
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(kTestResponse, challenge_response_);
  EXPECT_EQ(kTestCertificate, certificate_);
  EXPECT_EQ(1, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, SuccessWithAlwaysAskConsent) {
  fake_delegate_.set_always_ask_required(true);
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(kTestResponse, challenge_response_);
  EXPECT_EQ(kTestCertificate, certificate_);
  EXPECT_EQ(1, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, SuccessWithAttestationConsent) {
  fake_cryptohome_client_.set_attestation_enrolled(false);
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(kTestResponse, challenge_response_);
  EXPECT_EQ(kTestCertificate, certificate_);
  EXPECT_EQ(1, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, ConsentRejected) {
  fake_delegate_.set_always_ask_required(true);
  fake_delegate_.set_response(PlatformVerificationFlow::CONSENT_RESPONSE_DENY);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::USER_REJECTED, result_);
  EXPECT_EQ(1, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, FeatureDisabled) {
  fake_delegate_.set_attestation_disabled(true);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::POLICY_REJECTED, result_);
  EXPECT_EQ(0, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, NotVerified) {
  certificate_success_ = false;
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::PLATFORM_NOT_VERIFIED, result_);
}

TEST_F(PlatformVerificationFlowTest, ChallengeSigningError) {
  sign_challenge_success_ = false;
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::INTERNAL_ERROR, result_);
}

TEST_F(PlatformVerificationFlowTest, DBusFailure) {
  fake_cryptohome_client_.set_call_status(DBUS_METHOD_CALL_FAILURE);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::INTERNAL_ERROR, result_);
}

TEST_F(PlatformVerificationFlowTest, UpdateSettingsFailure) {
  fake_delegate_.set_origin_consent_required(true);
  fake_delegate_.set_update_settings_result(false);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::INTERNAL_ERROR, result_);
}

TEST_F(PlatformVerificationFlowTest, ConsentNoResponse) {
  fake_delegate_.set_response(PlatformVerificationFlow::CONSENT_RESPONSE_NONE);
  fake_delegate_.set_origin_consent_required(true);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::USER_REJECTED, result_);
}

}  // namespace attestation
}  // namespace chromeos
