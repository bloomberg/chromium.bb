// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_enrollment_handler.h"

#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/policy/fake_consumer_management_service.h"
#include "chrome/browser/chromeos/policy/fake_device_cloud_policy_initializer.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char* kTestOwner = "test@chromium.org.test";
}

namespace policy {

class ConsumerEnrollmentHandlerTest : public BrowserWithTestWindowTest {
 public:
  ConsumerEnrollmentHandlerTest()
      : fake_initializer_(new FakeDeviceCloudPolicyInitializer()),
        fake_service_(new FakeConsumerManagementService()) {
    // Set up FakeConsumerManagementService.
    fake_service_->set_status(ConsumerManagementService::STATUS_ENROLLING);
    fake_service_->SetEnrollmentStage(
        ConsumerManagementService::ENROLLMENT_STAGE_OWNER_STORED);

    // Inject fake objects.
    BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    connector->SetConsumerManagementServiceForTesting(
        make_scoped_ptr(fake_service_));
    connector->SetDeviceCloudPolicyInitializerForTesting(
        make_scoped_ptr(fake_initializer_));
  }

  virtual void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // Set up TestingProfileManager. This is required for NotificationUIManager.
    testing_profile_manager_.reset(new TestingProfileManager(
        TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(testing_profile_manager_->SetUp());

    // Set up FakeProfileOAuth2TokenService and issue a fake refresh token.
    ProfileOAuth2TokenServiceFactory::GetInstance()->SetTestingFactory(
        profile(), &BuildAutoIssuingFakeProfileOAuth2TokenService);
    GetFakeProfileOAuth2TokenService()->
        IssueRefreshTokenForUser(kTestOwner, "fake_token");

    // Set up the authenticated user name and ID.
    SigninManagerFactory::GetForProfile(profile())->
        SetAuthenticatedUsername(kTestOwner);
  }

  virtual void TearDown() override {
    g_browser_process->notification_ui_manager()->CancelAll();
    testing_profile_manager_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  FakeProfileOAuth2TokenService* GetFakeProfileOAuth2TokenService() {
    return static_cast<FakeProfileOAuth2TokenService*>(
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile()));
  }

  bool HasEnrollmentNotification() {
    return g_browser_process->notification_ui_manager()->FindById(
        "consumer_management.enroll",
        NotificationUIManager::GetProfileID(profile()));
  }

  void RunEnrollmentTest() {
    handler_.reset(
        new ConsumerEnrollmentHandler(profile(), fake_service_, NULL));
    base::RunLoop().RunUntilIdle();
  }

  FakeDeviceCloudPolicyInitializer* fake_initializer_;
  FakeConsumerManagementService* fake_service_;
  scoped_ptr<TestingProfileManager> testing_profile_manager_;
  scoped_ptr<ConsumerEnrollmentHandler> handler_;
};

TEST_F(ConsumerEnrollmentHandlerTest, EnrollsSuccessfully) {
  EXPECT_FALSE(fake_initializer_->was_start_enrollment_called());
  EXPECT_FALSE(HasEnrollmentNotification());

  RunEnrollmentTest();

  EXPECT_TRUE(fake_initializer_->was_start_enrollment_called());
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_SUCCESS,
            fake_service_->enrollment_stage_before_reset());
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_NONE,
            fake_service_->GetEnrollmentStage());
  EXPECT_TRUE(HasEnrollmentNotification());
}

TEST_F(ConsumerEnrollmentHandlerTest, FailsToGetAccessToken) {
  // Disable auto-posting so that RunEnrollmentTest() should stop and wait for
  // the access token to be available.
  GetFakeProfileOAuth2TokenService()->
      set_auto_post_fetch_response_on_message_loop(false);
  EXPECT_FALSE(HasEnrollmentNotification());

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
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_GET_TOKEN_FAILED,
            fake_service_->enrollment_stage_before_reset());
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_NONE,
            fake_service_->GetEnrollmentStage());
  EXPECT_TRUE(HasEnrollmentNotification());
}

TEST_F(ConsumerEnrollmentHandlerTest, FailsToRegister) {
  EXPECT_FALSE(fake_initializer_->was_start_enrollment_called());
  fake_initializer_->set_enrollment_status(EnrollmentStatus::ForStatus(
      EnrollmentStatus::STATUS_REGISTRATION_FAILED));
  EXPECT_FALSE(HasEnrollmentNotification());

  RunEnrollmentTest();

  EXPECT_TRUE(fake_initializer_->was_start_enrollment_called());
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_DM_SERVER_FAILED,
            fake_service_->enrollment_stage_before_reset());
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_NONE,
            fake_service_->GetEnrollmentStage());
  EXPECT_TRUE(HasEnrollmentNotification());
}

TEST_F(ConsumerEnrollmentHandlerTest,
       ShowsDesktopNotificationOnlyIfEnrollmentIsAlreadyCompleted) {
  fake_service_->set_status(ConsumerManagementService::STATUS_UNENROLLED);
  fake_service_->SetEnrollmentStage(
      ConsumerManagementService::ENROLLMENT_STAGE_CANCELED);
  EXPECT_FALSE(HasEnrollmentNotification());

  RunEnrollmentTest();

  EXPECT_FALSE(fake_initializer_->was_start_enrollment_called());
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_NONE,
            fake_service_->GetEnrollmentStage());
  EXPECT_TRUE(HasEnrollmentNotification());
}

}  // namespace policy
