// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_enrollment_handler.h"

#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/policy/fake_device_cloud_policy_initializer.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::Return;

namespace {
const char* kTestOwner = "test@chromium.org.test";
}

namespace policy {

class ConsumerEnrollmentHandlerTest : public BrowserWithTestWindowTest {
 public:
  ConsumerEnrollmentHandlerTest()
      : mock_user_manager_(new NiceMock<chromeos::MockUserManager>()),
        scoped_user_manager_enabler_(mock_user_manager_),
        fake_initializer_(new FakeDeviceCloudPolicyInitializer()) {
    // Set up MockUserManager. The first user will be the owner.
    mock_user_manager_->AddUser(kTestOwner);

    // Return false for IsCurrentUserOwner() so that the enrollment stage is not
    // reset.
    ON_CALL(*mock_user_manager_, IsCurrentUserOwner())
        .WillByDefault(Return(false));

    // Inject FakeDeviceCloudPolicyInitializer.
    BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    connector->SetDeviceCloudPolicyInitializerForTesting(
        scoped_ptr<DeviceCloudPolicyInitializer>(fake_initializer_));
  }

  virtual void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    testing_profile_manager_.reset(new TestingProfileManager(
        TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(testing_profile_manager_->SetUp());

    service_.reset(new ConsumerManagementService(NULL, NULL));
    handler_.reset(new ConsumerEnrollmentHandler(service_.get(), NULL));

    // Set up the testing profile.
    profile()->set_profile_name(kTestOwner);

    // Set up FakeProfileOAuth2TokenService and issue a fake refresh token.
    ProfileOAuth2TokenServiceFactory::GetInstance()->SetTestingFactory(
        profile(), &BuildAutoIssuingFakeProfileOAuth2TokenService);
    GetFakeProfileOAuth2TokenService()->
        IssueRefreshTokenForUser(kTestOwner, "fake_token");

    // Set up the authenticated user name and ID.
    SigninManagerFactory::GetForProfile(profile())->
        SetAuthenticatedUsername(kTestOwner);

    // The service should continue the enrollment process if the stage is
    // ENROLLMENT_STAGE_OWNER_STORED.
    SetEnrollmentStage(
        ConsumerManagementService::ENROLLMENT_STAGE_OWNER_STORED);
  }

  virtual void TearDown() override {
    g_browser_process->notification_ui_manager()->CancelAll();
    testing_profile_manager_.reset();
    handler_.reset();
    service_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  ConsumerManagementService::EnrollmentStage GetEnrollmentStage() {
    return static_cast<ConsumerManagementService::EnrollmentStage>(
        g_browser_process->local_state()->GetInteger(
            prefs::kConsumerManagementEnrollmentStage));
  }

  void SetEnrollmentStage(ConsumerManagementService::EnrollmentStage stage) {
    g_browser_process->local_state()->SetInteger(
        prefs::kConsumerManagementEnrollmentStage, stage);
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
    // Send the profile prepared notification to continue the enrollment.
    handler_->Observe(chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                      content::Source<void>(NULL),  // Not used.
                      content::Details<Profile>(profile()));
    base::RunLoop().RunUntilIdle();
  }

  NiceMock<chromeos::MockUserManager>* mock_user_manager_;
  chromeos::ScopedUserManagerEnabler scoped_user_manager_enabler_;
  FakeDeviceCloudPolicyInitializer* fake_initializer_;
  scoped_ptr<ConsumerManagementService> service_;
  scoped_ptr<ConsumerEnrollmentHandler> handler_;
  scoped_ptr<TestingProfileManager> testing_profile_manager_;
};

TEST_F(ConsumerEnrollmentHandlerTest, EnrollsSuccessfully) {
  EXPECT_FALSE(fake_initializer_->was_start_enrollment_called());

  RunEnrollmentTest();

  EXPECT_TRUE(fake_initializer_->was_start_enrollment_called());
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_SUCCESS,
            GetEnrollmentStage());
  EXPECT_FALSE(HasEnrollmentNotification());
}

TEST_F(ConsumerEnrollmentHandlerTest,
       ShowsDesktopNotificationAndResetsEnrollmentStageIfCurrentUserIsOwner) {
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_OWNER_STORED,
            GetEnrollmentStage());
  EXPECT_FALSE(HasEnrollmentNotification());
  EXPECT_CALL(*mock_user_manager_, IsCurrentUserOwner())
      .WillOnce(Return(true));

  RunEnrollmentTest();

  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_NONE,
            GetEnrollmentStage());
  EXPECT_TRUE(HasEnrollmentNotification());
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
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_GET_TOKEN_FAILED,
            GetEnrollmentStage());
}

TEST_F(ConsumerEnrollmentHandlerTest, FailsToRegister) {
  EXPECT_FALSE(fake_initializer_->was_start_enrollment_called());
  fake_initializer_->set_enrollment_status(EnrollmentStatus::ForStatus(
      EnrollmentStatus::STATUS_REGISTRATION_FAILED));

  RunEnrollmentTest();

  EXPECT_TRUE(fake_initializer_->was_start_enrollment_called());
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_DM_SERVER_FAILED,
            GetEnrollmentStage());
}

TEST_F(ConsumerEnrollmentHandlerTest,
       ShowsDesktopNotificationOnlyIfEnrollmentIsAlreadyCompleted) {
  SetEnrollmentStage(ConsumerManagementService::ENROLLMENT_STAGE_CANCELED);
  EXPECT_FALSE(HasEnrollmentNotification());

  RunEnrollmentTest();

  EXPECT_FALSE(fake_initializer_->was_start_enrollment_called());
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_STAGE_NONE,
            GetEnrollmentStage());
  EXPECT_TRUE(HasEnrollmentNotification());
}

}  // namespace policy
