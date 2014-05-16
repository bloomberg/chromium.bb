// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/attestation/attestation_signed_data.pb.h"
#include "chrome/browser/chromeos/attestation/fake_certificate.h"
#include "chrome/browser/chromeos/attestation/platform_verification_flow.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/renderer_host/pepper/device_id_fetcher.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/settings/cros_settings_names.h"
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
const char kTestURLSecure[] = "https://mytestdomain/test";
const char kTestURLExtension[] = "chrome-extension://mytestextension";

class FakeDelegate : public PlatformVerificationFlow::Delegate {
 public:
  FakeDelegate() : response_(PlatformVerificationFlow::CONSENT_RESPONSE_ALLOW),
                   num_consent_calls_(0),
                   url_(kTestURL),
                   is_incognito_(false) {
    // Configure a user for the mock user manager.
    mock_user_manager_.SetActiveUser(kTestEmail);
  }
  virtual ~FakeDelegate() {}

  void SetUp() {
    ProfileImpl::RegisterProfilePrefs(pref_service_.registry());
    chrome::DeviceIDFetcher::RegisterProfilePrefs(pref_service_.registry());
    PlatformVerificationFlow::RegisterProfilePrefs(pref_service_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(pref_service_.registry());
    content_settings_ = new HostContentSettingsMap(&pref_service_, false);
  }

  void TearDown() {
    content_settings_->ShutdownOnUIThread();
  }

  virtual void ShowConsentPrompt(
      content::WebContents* web_contents,
      const PlatformVerificationFlow::Delegate::ConsentCallback& callback)
      OVERRIDE {
    num_consent_calls_++;
    callback.Run(response_);
  }

  virtual PrefService* GetPrefs(content::WebContents* web_contents) OVERRIDE {
    return &pref_service_;
  }

  virtual const GURL& GetURL(content::WebContents* web_contents) OVERRIDE {
    return url_;
  }

  virtual User* GetUser(content::WebContents* web_contents) OVERRIDE {
    return mock_user_manager_.GetActiveUser();
  }

  virtual HostContentSettingsMap* GetContentSettings(
      content::WebContents* web_contents) OVERRIDE {
    return content_settings_;
  }

  virtual bool IsGuestOrIncognito(content::WebContents* web_contents) OVERRIDE {
    return is_incognito_;
  }

  void set_response(PlatformVerificationFlow::ConsentResponse response) {
    response_ = response;
  }

  int num_consent_calls() {
    return num_consent_calls_;
  }

  TestingPrefServiceSyncable& pref_service() {
    return pref_service_;
  }

  void set_url(const GURL& url) {
    url_ = url;
  }

  void set_is_incognito(bool is_incognito) {
    is_incognito_ = is_incognito;
  }

 private:
  PlatformVerificationFlow::ConsentResponse response_;
  int num_consent_calls_;
  TestingPrefServiceSyncable pref_service_;
  MockUserManager mock_user_manager_;
  GURL url_;
  scoped_refptr<HostContentSettingsMap> content_settings_;
  bool is_incognito_;

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
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        certificate_success_(true),
        fake_certificate_index_(0),
        sign_challenge_success_(true),
        result_(PlatformVerificationFlow::INTERNAL_ERROR) {}

  void SetUp() {
    fake_delegate_.SetUp();

    // Create a verifier for tests to call.
    verifier_ = new PlatformVerificationFlow(&mock_attestation_flow_,
                                             &mock_async_caller_,
                                             &fake_cryptohome_client_,
                                             &fake_delegate_);

    // Create callbacks for tests to use with verifier_.
    callback_ = base::Bind(&PlatformVerificationFlowTest::FakeChallengeCallback,
                           base::Unretained(this));

    // Configure the global cros_settings.
    CrosSettings* cros_settings = CrosSettings::Get();
    device_settings_provider_ =
        cros_settings->GetProvider(kAttestationForContentProtectionEnabled);
    cros_settings->RemoveSettingsProvider(device_settings_provider_);
    cros_settings->AddSettingsProvider(&stub_settings_provider_);
    cros_settings->SetBoolean(kAttestationForContentProtectionEnabled, true);

    // Start with the first-time setting set since most tests want this.
    fake_delegate_.pref_service().SetUserPref(prefs::kRAConsentFirstTime,
                                              new base::FundamentalValue(true));

  }

  void TearDown() {
    // Restore the real DeviceSettingsProvider.
    CrosSettings* cros_settings = CrosSettings::Get();
    cros_settings->RemoveSettingsProvider(&stub_settings_provider_);
    cros_settings->AddSettingsProvider(device_settings_provider_);
    fake_delegate_.TearDown();
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
                TpmAttestationSignSimpleChallenge(KEY_USER, kTestEmail,
                                                  expected_key_name,
                                                  kTestChallenge, _))
        .WillRepeatedly(WithArgs<4>(Invoke(
            this, &PlatformVerificationFlowTest::FakeSignChallenge)));
  }

  void SetUserConsent(const GURL& url, bool allow) {
    verifier_->RecordDomainConsent(fake_delegate_.GetContentSettings(NULL),
                                   url,
                                   allow);
  }

  void FakeGetCertificate(
      const AttestationFlow::CertificateCallback& callback) {
    std::string certificate =
        (fake_certificate_index_ < fake_certificate_list_.size()) ?
            fake_certificate_list_[fake_certificate_index_] : kTestCertificate;
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback,
                                                      certificate_success_,
                                                      certificate));
    ++fake_certificate_index_;
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

  std::string CreateFakeResponseProto() {
    SignedData pb;
    pb.set_data(kTestSignedData);
    pb.set_signature(kTestSignature);
    std::string serial;
    CHECK(pb.SerializeToString(&serial));
    return serial;
  }

 protected:
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  StrictMock<MockAttestationFlow> mock_attestation_flow_;
  cryptohome::MockAsyncMethodCaller mock_async_caller_;
  CustomFakeCryptohomeClient fake_cryptohome_client_;
  FakeDelegate fake_delegate_;
  CrosSettingsProvider* device_settings_provider_;
  StubCrosSettingsProvider stub_settings_provider_;
  ScopedTestDeviceSettingsService test_device_settings_service_;
  ScopedTestCrosSettings test_cros_settings_;
  scoped_refptr<PlatformVerificationFlow> verifier_;

  // Controls result of FakeGetCertificate.
  bool certificate_success_;
  std::vector<std::string> fake_certificate_list_;
  size_t fake_certificate_index_;

  // Controls result of FakeSignChallenge.
  bool sign_challenge_success_;

  // Callback functions and data.
  PlatformVerificationFlow::ChallengeCallback callback_;
  PlatformVerificationFlow::Result result_;
  std::string challenge_salt_;
  std::string challenge_signature_;
  std::string certificate_;
};

TEST_F(PlatformVerificationFlowTest, SuccessNoConsent) {
  SetUserConsent(GURL(kTestURL), true);
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

TEST_F(PlatformVerificationFlowTest, SuccessWithAttestationConsent) {
  SetUserConsent(GURL(kTestURL), true);
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
  SetUserConsent(GURL(kTestURL), true);
  fake_delegate_.pref_service().SetUserPref(prefs::kRAConsentFirstTime,
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
  fake_delegate_.pref_service().SetUserPref(prefs::kEnableDRM,
                                            new base::FundamentalValue(false));
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::POLICY_REJECTED, result_);
  EXPECT_EQ(0, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, FeatureDisabledByUserForDomain) {
  SetUserConsent(GURL(kTestURL), false);
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
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::USER_REJECTED, result_);
}

TEST_F(PlatformVerificationFlowTest, ConsentPerScheme) {
  fake_delegate_.set_response(PlatformVerificationFlow::CONSENT_RESPONSE_DENY);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::USER_REJECTED, result_);
  // Call again and expect denial based on previous response.
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::POLICY_REJECTED, result_);
  // Call with a different scheme and expect another consent prompt.
  fake_delegate_.set_url(GURL(kTestURLSecure));
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::USER_REJECTED, result_);
  EXPECT_EQ(2, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, ConsentForExtension) {
  fake_delegate_.set_response(PlatformVerificationFlow::CONSENT_RESPONSE_DENY);
  fake_delegate_.set_url(GURL(kTestURLExtension));
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::USER_REJECTED, result_);
  EXPECT_EQ(1, fake_delegate_.num_consent_calls());
}

TEST_F(PlatformVerificationFlowTest, Timeout) {
  verifier_->set_timeout_delay(base::TimeDelta::FromSeconds(0));
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::TIMEOUT, result_);
}

TEST_F(PlatformVerificationFlowTest, ExpiredCert) {
  ExpectAttestationFlow();
  fake_certificate_list_.resize(2);
  ASSERT_TRUE(GetFakeCertificate(base::TimeDelta::FromDays(-1),
                                 &fake_certificate_list_[0]));
  ASSERT_TRUE(GetFakeCertificate(base::TimeDelta::FromDays(1),
                                 &fake_certificate_list_[1]));
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(certificate_, fake_certificate_list_[1]);
}

TEST_F(PlatformVerificationFlowTest, IncognitoMode) {
  fake_delegate_.set_is_incognito(true);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::PLATFORM_NOT_VERIFIED, result_);
}

}  // namespace attestation
}  // namespace chromeos
