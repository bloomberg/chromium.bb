// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/attestation/attestation_signed_data.pb.h"
#include "chrome/browser/chromeos/attestation/platform_verification_flow.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/common/pref_names.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/system/mock_statistics_provider.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::WithArgs;

namespace chromeos {
namespace attestation {

namespace {

const char kTestID[] = "test_id";
const char kTestChallenge[] = "test_challenge";
const char kTestSignedData[] = "test_challenge_with_salt";
const char kTestSignature[] = "test_signature";
const char kTestCertificate[] = "test_certificate";
const char kTestEmail[] = "test_email@chromium.org";
const char kTestURL[] = "http://mytestdomain/test";
const char kTestDomain[] = "mytestdomain";

class FakeDelegate : public PlatformVerificationFlow::Delegate {
 public:
  FakeDelegate() : response_(PlatformVerificationFlow::CONSENT_RESPONSE_ALLOW),
                   num_consent_calls_(0) {}
  virtual ~FakeDelegate() {}

  virtual void ShowConsentPrompt(
      PlatformVerificationFlow::ConsentType type,
      content::WebContents* web_contents,
      const PlatformVerificationFlow::Delegate::ConsentCallback& callback)
      OVERRIDE {
    num_consent_calls_++;
    callback.Run(response_);
  }

  void set_response(PlatformVerificationFlow::ConsentResponse response) {
    response_ = response;
  }

  int num_consent_calls() {
    return num_consent_calls_;
  }

 private:
  PlatformVerificationFlow::ConsentResponse response_;
  int num_consent_calls_;

  DISALLOW_COPY_AND_ASSIGN(FakeDelegate);
};

class CustomFakeCryptohomeClient : public FakeCryptohomeClient {
 public:
  CustomFakeCryptohomeClient() : call_status_(DBUS_METHOD_CALL_SUCCESS),
                                 attestation_enrolled_(true),
                                 attestation_prepared_(true) {}
  virtual void TpmAttestationIsEnrolled(
      const BoolDBusMethodCallback& callback) OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback,
                                                      call_status_,
                                                      attestation_enrolled_));
  }

  virtual void TpmAttestationIsPrepared(
      const BoolDBusMethodCallback& callback) OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback,
                                                      call_status_,
                                                      attestation_prepared_));
  }

  void set_call_status(DBusMethodCallStatus call_status) {
    call_status_ = call_status;
  }

  void set_attestation_enrolled(bool attestation_enrolled) {
    attestation_enrolled_ = attestation_enrolled;
  }

  void set_attestation_prepared(bool attestation_prepared) {
    attestation_prepared_ = attestation_prepared;
  }

 private:
  DBusMethodCallStatus call_status_;
  bool attestation_enrolled_;
  bool attestation_prepared_;
};

}  // namespace

class PlatformVerificationFlowTest : public ::testing::Test {
 public:
  PlatformVerificationFlowTest()
      : message_loop_(base::MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        certificate_success_(true),
        sign_challenge_success_(true),
        result_(PlatformVerificationFlow::INTERNAL_ERROR),
        check_state_result_(false) {}

  void SetUp() {
    // Configure a user for the mock user manager.
    mock_user_manager_.SetActiveUser(kTestEmail);

    // Configure the statistics provider to report verified mode.
    EXPECT_CALL(mock_statistics_provider_,
                GetMachineStatistic(system::kDevSwitchBootMode, _))
        .WillRepeatedly(DoAll(SetArgumentPointee<1>(std::string("0")),
                              Return(true)));

    // Create a verifier for tests to call.
    verifier_.reset(new PlatformVerificationFlow(&mock_attestation_flow_,
                                                 &mock_async_caller_,
                                                 &fake_cryptohome_client_,
                                                 &mock_user_manager_,
                                                 &mock_statistics_provider_,
                                                 &fake_delegate_));

    // Create callbacks for tests to use with verifier_.
    callback_ = base::Bind(&PlatformVerificationFlowTest::FakeChallengeCallback,
                           base::Unretained(this));
    check_state_callback_ = base::Bind(
        &PlatformVerificationFlowTest::FakeCheckStateCallback,
        base::Unretained(this));

    // Configure the test pref service.
    pref_service_.registry()->RegisterBooleanPref(prefs::kEnableDRM, true);
    pref_service_.registry()->RegisterBooleanPref(prefs::kRAConsentFirstTime,
                                                  true);
    pref_service_.registry()->RegisterBooleanPref(prefs::kRAConsentAlways,
                                                  false);
    pref_service_.registry()->RegisterDictionaryPref(prefs::kRAConsentDomains);
    verifier_->set_testing_prefs(&pref_service_);

    // Configure the global cros_settings.
    CrosSettings* cros_settings = CrosSettings::Get();
    device_settings_provider_ =
        cros_settings->GetProvider(kAttestationForContentProtectionEnabled);
    cros_settings->RemoveSettingsProvider(device_settings_provider_);
    cros_settings->AddSettingsProvider(&stub_settings_provider_);
    cros_settings->SetBoolean(kAttestationForContentProtectionEnabled, true);

    // Configure a test URL to shortcut the dependency on WebContents.
    verifier_->set_testing_url(GURL(kTestURL));
  }

  void TearDown() {
    verifier_.reset();
    // Restore the real DeviceSettingsProvider.
    CrosSettings* cros_settings = CrosSettings::Get();
    cros_settings->RemoveSettingsProvider(&stub_settings_provider_);
    cros_settings->AddSettingsProvider(device_settings_provider_);
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
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   sign_challenge_success_,
                   CreateFakeResponseProto()));
  }

  void FakeChallengeCallback(PlatformVerificationFlow::Result result,
                             const std::string& salt,
                             const std::string& signature,
                             const std::string& certificate) {
    result_ = result;
    challenge_salt_ = salt;
    challenge_signature_ = signature;
    certificate_ = certificate;
  }

  void FakeCheckStateCallback(bool result) {
    check_state_result_ = result;
  }

  std::string CreateFakeResponseProto() {
    SignedData pb;
    pb.set_data(kTestSignedData);
    pb.set_signature(kTestSignature);
    std::string serial;
    CHECK(pb.SerializeToString(&serial));
    return serial;
  }

 protected:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  StrictMock<MockAttestationFlow> mock_attestation_flow_;
  cryptohome::MockAsyncMethodCaller mock_async_caller_;
  CustomFakeCryptohomeClient fake_cryptohome_client_;
  MockUserManager mock_user_manager_;
  system::MockStatisticsProvider mock_statistics_provider_;
  FakeDelegate fake_delegate_;
  TestingPrefServiceSimple pref_service_;
  CrosSettingsProvider* device_settings_provider_;
  StubCrosSettingsProvider stub_settings_provider_;
  ScopedTestDeviceSettingsService test_device_settings_service_;
  ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<PlatformVerificationFlow> verifier_;

  // Controls result of FakeGetCertificate.
  bool certificate_success_;

  // Controls result of FakeSignChallenge.
  bool sign_challenge_success_;

  // Callback functions and data.
  PlatformVerificationFlow::ChallengeCallback callback_;
  PlatformVerificationFlow::Result result_;
  std::string challenge_salt_;
  std::string challenge_signature_;
  std::string certificate_;
  base::Callback<void(bool result)> check_state_callback_;
  bool check_state_result_;
};

TEST_F(PlatformVerificationFlowTest, SuccessNoConsent) {
  // Make sure the call will fail if consent is requested.
  fake_delegate_.set_response(PlatformVerificationFlow::CONSENT_RESPONSE_DENY);
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(kTestSignedData, challenge_salt_);
  EXPECT_EQ(kTestSignature, challenge_signature_);
  EXPECT_EQ(kTestCertificate, certificate_);
  EXPECT_EQ(0, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, SuccessWithAlwaysAskConsent) {
  pref_service_.SetUserPref(prefs::kRAConsentAlways,
                            new base::FundamentalValue(true));
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(kTestSignedData, challenge_salt_);
  EXPECT_EQ(kTestSignature, challenge_signature_);
  EXPECT_EQ(kTestCertificate, certificate_);
  EXPECT_EQ(1, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, SuccessWithAttestationConsent) {
  fake_cryptohome_client_.set_attestation_enrolled(false);
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(kTestSignedData, challenge_salt_);
  EXPECT_EQ(kTestSignature, challenge_signature_);
  EXPECT_EQ(kTestCertificate, certificate_);
  EXPECT_EQ(1, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, SuccessWithFirstTimeConsent) {
  pref_service_.SetUserPref(prefs::kRAConsentFirstTime,
                            new base::FundamentalValue(false));
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(kTestSignedData, challenge_salt_);
  EXPECT_EQ(kTestSignature, challenge_signature_);
  EXPECT_EQ(kTestCertificate, certificate_);
  EXPECT_EQ(1, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, ConsentRejected) {
  pref_service_.SetUserPref(prefs::kRAConsentAlways,
                            new base::FundamentalValue(true));
  fake_delegate_.set_response(PlatformVerificationFlow::CONSENT_RESPONSE_DENY);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::USER_REJECTED, result_);
  EXPECT_EQ(1, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, FeatureDisabled) {
  CrosSettings::Get()->SetBoolean(kAttestationForContentProtectionEnabled,
                                  false);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::POLICY_REJECTED, result_);
  EXPECT_EQ(0, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, FeatureDisabledByUser) {
  pref_service_.SetUserPref(prefs::kEnableDRM,
                            new base::FundamentalValue(false));
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::POLICY_REJECTED, result_);
  EXPECT_EQ(0, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, FeatureDisabledByUserForDomain) {
  base::DictionaryValue* domains = new base::DictionaryValue();
  domains->SetBoolean(kTestDomain, false);
  pref_service_.SetUserPref(prefs::kRAConsentDomains, domains);
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

TEST_F(PlatformVerificationFlowTest, ConsentNoResponse) {
  fake_delegate_.set_response(PlatformVerificationFlow::CONSENT_RESPONSE_NONE);
  pref_service_.SetUserPref(prefs::kRAConsentAlways,
                            new base::FundamentalValue(true));
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::USER_REJECTED, result_);
}

TEST_F(PlatformVerificationFlowTest, FastCheck) {
  verifier_->CheckPlatformState(check_state_callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(check_state_result_);
}

TEST_F(PlatformVerificationFlowTest, FastCheckNoStat) {
  // Configure the stats provider to fail.
  EXPECT_CALL(mock_statistics_provider_,
              GetMachineStatistic(system::kDevSwitchBootMode, _))
      .WillRepeatedly(Return(false));

  verifier_->CheckPlatformState(check_state_callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_state_result_);
}

TEST_F(PlatformVerificationFlowTest, FastCheckStatDevMode) {
  // Configure the stats provider to fail.
  EXPECT_CALL(mock_statistics_provider_,
              GetMachineStatistic(system::kDevSwitchBootMode, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(std::string("1")),
                            Return(true)));

  verifier_->CheckPlatformState(check_state_callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_state_result_);
}

TEST_F(PlatformVerificationFlowTest, FastCheckStatInvalidMode) {
  // Configure the stats provider to fail.
  EXPECT_CALL(mock_statistics_provider_,
              GetMachineStatistic(system::kDevSwitchBootMode, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(std::string("INVALID")),
                            Return(true)));

  verifier_->CheckPlatformState(check_state_callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_state_result_);
}

TEST_F(PlatformVerificationFlowTest, FastCheckNoAttestation) {
  fake_cryptohome_client_.set_attestation_prepared(false);
  verifier_->CheckPlatformState(check_state_callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_state_result_);
}

TEST_F(PlatformVerificationFlowTest, FastCheckDBusFailure) {
  fake_cryptohome_client_.set_call_status(DBUS_METHOD_CALL_FAILURE);
  verifier_->CheckPlatformState(check_state_callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(check_state_result_);
}

}  // namespace attestation
}  // namespace chromeos
