// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_enrollment_handler.h"

#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/policy/consumer_management_stage.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/policy/fake_consumer_management_service.h"
#include "chrome/browser/chromeos/policy/fake_device_cloud_policy_initializer.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char* kTestOwner = "test.owner@chromium.org.test";
const char* kTestUser = "test.user@chromium.org.test";
}

namespace policy {

class ConsumerEnrollmentHandlerTest : public testing::Test {
 public:
  ConsumerEnrollmentHandlerTest()
      : fake_service_(new FakeConsumerManagementService()),
        fake_initializer_(new FakeDeviceCloudPolicyInitializer()),
        fake_user_manager_(new chromeos::FakeChromeUserManager()),
        scoped_user_manager_enabler_(fake_user_manager_),
        testing_profile_manager_(
            new TestingProfileManager(TestingBrowserProcess::GetGlobal())) {
    // Set up FakeConsumerManagementService.
    fake_service_->SetStatusAndStage(
        ConsumerManagementService::STATUS_ENROLLING,
        ConsumerManagementStage::EnrollmentOwnerStored());

    // Inject fake objects.
    BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    connector->SetConsumerManagementServiceForTesting(
        make_scoped_ptr(fake_service_));
    connector->SetDeviceCloudPolicyInitializerForTesting(
        make_scoped_ptr(fake_initializer_));

    // Set up FakeChromeUserManager.
    fake_user_manager_->AddUser(AccountId::FromUserEmail(kTestOwner));
    fake_user_manager_->AddUser(AccountId::FromUserEmail(kTestUser));
    fake_user_manager_->set_owner_id(AccountId::FromUserEmail(kTestOwner));
  }

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    TestingProfile::TestingFactories factories;
    factories.push_back(
        std::make_pair(ProfileOAuth2TokenServiceFactory::GetInstance(),
                       BuildAutoIssuingFakeProfileOAuth2TokenService));
    profile_ = testing_profile_manager_->CreateTestingProfile(
        kTestUser, scoped_ptr<syncable_prefs::PrefServiceSyncable>(),
        base::UTF8ToUTF16(kTestUser), 0, std::string(), factories);

    // Set up the authenticated user name and ID.
    SigninManagerFactory::GetForProfile(profile_)
        ->SetAuthenticatedAccountInfo(kTestOwner, kTestOwner);

    // Issue a fake refresh token.
    GetFakeProfileOAuth2TokenService()->UpdateCredentials(kTestOwner,
                                                          "fake_token");
  }

  FakeProfileOAuth2TokenService* GetFakeProfileOAuth2TokenService() {
    return static_cast<FakeProfileOAuth2TokenService*>(
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_));
  }

  void RunEnrollmentTest() {
    handler_.reset(
        new ConsumerEnrollmentHandler(profile_, fake_service_, NULL));
    base::RunLoop().RunUntilIdle();
  }

  content::TestBrowserThreadBundle thread_bundle;
  FakeConsumerManagementService* fake_service_;
  FakeDeviceCloudPolicyInitializer* fake_initializer_;
  chromeos::FakeChromeUserManager* fake_user_manager_;
  chromeos::ScopedUserManagerEnabler scoped_user_manager_enabler_;
  scoped_ptr<TestingProfileManager> testing_profile_manager_;
  Profile* profile_;
  scoped_ptr<ConsumerEnrollmentHandler> handler_;
};

TEST_F(ConsumerEnrollmentHandlerTest, EnrollsSuccessfully) {
  EXPECT_FALSE(fake_initializer_->was_start_enrollment_called());

  RunEnrollmentTest();

  EXPECT_TRUE(fake_initializer_->was_start_enrollment_called());
  EXPECT_EQ(ConsumerManagementStage::EnrollmentSuccess(),
            fake_service_->GetStage());
}

TEST_F(ConsumerEnrollmentHandlerTest, FailsToGetAccessToken) {
  // Disable auto-posting so that RunEnrollmentTest() should stop and wait for
  // the access token to be available.
  GetFakeProfileOAuth2TokenService()->
      set_auto_post_fetch_response_on_message_loop(false);

  RunEnrollmentTest();

  // The service should have a pending token request.
  OAuth2TokenService::Request* token_request =
      handler_->GetTokenRequestForTesting();
  EXPECT_TRUE(token_request);

  // Tell the service that the access token is not available because of some
  // backend issue.
  handler_->OnGetTokenFailure(
      token_request,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR));

  EXPECT_FALSE(fake_initializer_->was_start_enrollment_called());
  EXPECT_EQ(ConsumerManagementStage::EnrollmentGetTokenFailed(),
            fake_service_->GetStage());
}

TEST_F(ConsumerEnrollmentHandlerTest, FailsToRegister) {
  EXPECT_FALSE(fake_initializer_->was_start_enrollment_called());
  fake_initializer_->set_enrollment_status(EnrollmentStatus::ForStatus(
      EnrollmentStatus::STATUS_REGISTRATION_FAILED));

  RunEnrollmentTest();

  EXPECT_TRUE(fake_initializer_->was_start_enrollment_called());
  EXPECT_EQ(ConsumerManagementStage::EnrollmentDMServerFailed(),
            fake_service_->GetStage());
}

}  // namespace policy
