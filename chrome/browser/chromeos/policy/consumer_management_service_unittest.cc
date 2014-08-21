// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_management_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::_;

namespace {
const char* kAttributeOwnerId = "consumer_management.owner_id";
const char* kTestOwner = "test@chromium.org.test";
}

namespace policy {

class ConsumerManagementServiceTest : public testing::Test {
 public:
  ConsumerManagementServiceTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()),
        cryptohome_result_(false),
        set_owner_status_(false) {
    ON_CALL(mock_cryptohome_client_, GetBootAttribute(_, _))
        .WillByDefault(
            Invoke(this, &ConsumerManagementServiceTest::MockGetBootAttribute));
    ON_CALL(mock_cryptohome_client_, SetBootAttribute(_, _))
        .WillByDefault(
            Invoke(this, &ConsumerManagementServiceTest::MockSetBootAttribute));
    ON_CALL(mock_cryptohome_client_, FlushAndSignBootAttributes(_, _))
        .WillByDefault(
            Invoke(this,
                   &ConsumerManagementServiceTest::
                       MockFlushAndSignBootAttributes));
  }

  virtual void SetUp() OVERRIDE {
    service_.reset(new ConsumerManagementService(&mock_cryptohome_client_));
  }

  virtual void TearDown() OVERRIDE {
    service_.reset();
  }

  ConsumerManagementService::ConsumerEnrollmentState GetEnrollmentState() {
    return static_cast<ConsumerManagementService::ConsumerEnrollmentState>(
        testing_local_state_.Get()->GetInteger(
            prefs::kConsumerManagementEnrollmentState));
  }

  void SetEnrollmentState(
      ConsumerManagementService::ConsumerEnrollmentState state) {
    testing_local_state_.Get()->SetInteger(
        prefs::kConsumerManagementEnrollmentState, state);
  }

  void MockGetBootAttribute(
      const cryptohome::GetBootAttributeRequest& request,
      const chromeos::CryptohomeClient::ProtobufMethodCallback& callback) {
    get_boot_attribute_request_ = request;
    callback.Run(cryptohome_status_, cryptohome_result_, cryptohome_reply_);
  }

  void MockSetBootAttribute(
      const cryptohome::SetBootAttributeRequest& request,
      const chromeos::CryptohomeClient::ProtobufMethodCallback& callback) {
    set_boot_attribute_request_ = request;
    callback.Run(cryptohome_status_, cryptohome_result_, cryptohome_reply_);
  }

  void MockFlushAndSignBootAttributes(
      const cryptohome::FlushAndSignBootAttributesRequest& request,
      const chromeos::CryptohomeClient::ProtobufMethodCallback& callback) {
    callback.Run(cryptohome_status_, cryptohome_result_, cryptohome_reply_);
  }

  void OnGetOwnerDone(const std::string& owner) {
    owner_ = owner;
  }

  void OnSetOwnerDone(bool status) {
    set_owner_status_ = status;
  }

  ScopedTestingLocalState testing_local_state_;
  NiceMock<chromeos::MockCryptohomeClient> mock_cryptohome_client_;
  scoped_ptr<ConsumerManagementService> service_;

  chromeos::DBusMethodCallStatus cryptohome_status_;
  bool cryptohome_result_;
  cryptohome::BaseReply cryptohome_reply_;
  cryptohome::GetBootAttributeRequest get_boot_attribute_request_;
  cryptohome::SetBootAttributeRequest set_boot_attribute_request_;

  std::string owner_;
  bool set_owner_status_;
};

TEST_F(ConsumerManagementServiceTest, CanGetEnrollmentState) {
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_NONE,
            service_->GetEnrollmentState());

  SetEnrollmentState(ConsumerManagementService::ENROLLMENT_REQUESTED);

  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_REQUESTED,
            service_->GetEnrollmentState());
}

TEST_F(ConsumerManagementServiceTest, CanSetEnrollmentState) {
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_NONE, GetEnrollmentState());

  service_->SetEnrollmentState(ConsumerManagementService::ENROLLMENT_REQUESTED);

  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_REQUESTED,
            GetEnrollmentState());
}

TEST_F(ConsumerManagementServiceTest, CanGetOwner) {
  cryptohome_status_ = chromeos::DBUS_METHOD_CALL_SUCCESS;
  cryptohome_result_ = true;
  cryptohome_reply_.MutableExtension(cryptohome::GetBootAttributeReply::reply)->
      set_value(kTestOwner);

  service_->GetOwner(base::Bind(&ConsumerManagementServiceTest::OnGetOwnerDone,
                                base::Unretained(this)));

  EXPECT_EQ(kAttributeOwnerId, get_boot_attribute_request_.name());
  EXPECT_EQ(kTestOwner, owner_);
}

TEST_F(ConsumerManagementServiceTest, GetOwnerReturnsAnEmptyStringWhenItFails) {
  cryptohome_status_ = chromeos::DBUS_METHOD_CALL_FAILURE;
  cryptohome_result_ = false;
  cryptohome_reply_.MutableExtension(cryptohome::GetBootAttributeReply::reply)->
      set_value(kTestOwner);

  service_->GetOwner(base::Bind(&ConsumerManagementServiceTest::OnGetOwnerDone,
                                base::Unretained(this)));

  EXPECT_EQ("", owner_);
}

TEST_F(ConsumerManagementServiceTest, CanSetOwner) {
  cryptohome_status_ = chromeos::DBUS_METHOD_CALL_SUCCESS;
  cryptohome_result_ = true;

  service_->SetOwner(kTestOwner,
                     base::Bind(&ConsumerManagementServiceTest::OnSetOwnerDone,
                                base::Unretained(this)));

  EXPECT_EQ(kAttributeOwnerId, set_boot_attribute_request_.name());
  EXPECT_EQ(kTestOwner, set_boot_attribute_request_.value());
  EXPECT_TRUE(set_owner_status_);
}

TEST_F(ConsumerManagementServiceTest, SetOwnerReturnsFalseWhenItFails) {
  cryptohome_status_ = chromeos::DBUS_METHOD_CALL_FAILURE;
  cryptohome_result_ = false;

  service_->SetOwner(kTestOwner,
                     base::Bind(&ConsumerManagementServiceTest::OnSetOwnerDone,
                                base::Unretained(this)));

  EXPECT_FALSE(set_owner_status_);
}

class ConsumerManagementServiceEnrollmentTest
    : public ConsumerManagementServiceTest {
 public:
  ConsumerManagementServiceEnrollmentTest()
      : mock_user_manager_(new NiceMock<chromeos::MockUserManager>()),
        scoped_user_manager_enabler_(mock_user_manager_),
        fake_initializer_(new FakeDeviceCloudPolicyInitializer()),
        enrollment_status_(EnrollmentStatus::ForStatus(
            EnrollmentStatus::STATUS_SUCCESS)){
    // Set up MockUserManager. The first user will be the owner.
    mock_user_manager_->AddUser(kTestOwner);

    // Return false for IsCurrentUserOwner() so that the enrollment state is not
    // reset.
    ON_CALL(*mock_user_manager_, IsCurrentUserOwner())
        .WillByDefault(Return(false));

    // Create test profile and set up FakeProfileOAuth2TokenService.
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              &BuildAutoIssuingFakeProfileOAuth2TokenService);
    testing_owner_profile_ = builder.Build();
    testing_owner_profile_->set_profile_name(kTestOwner);

    // Set up the authenticated user name and ID.
    SigninManagerFactory::GetForProfile(testing_owner_profile_.get())->
        SetAuthenticatedUsername(kTestOwner);

    // Issue a fake refresh token.
    GetFakeProfileOAuth2TokenService()->
        IssueRefreshTokenForUser(kTestOwner, "fake_token");

    // Inject FakeDeviceCloudPolicyInitializer.
    BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    connector->SetDeviceCloudPolicyInitializerForTesting(
        scoped_ptr<DeviceCloudPolicyInitializer>(fake_initializer_));

    // The service should continue the enrollment process if the state is
    // ENROLLMENT_OWNER_STORED.
    SetEnrollmentState(ConsumerManagementService::ENROLLMENT_OWNER_STORED);
  }

  FakeProfileOAuth2TokenService* GetFakeProfileOAuth2TokenService() {
    return static_cast<FakeProfileOAuth2TokenService*>(
        ProfileOAuth2TokenServiceFactory::GetForProfile(
            testing_owner_profile_.get()));
  }

  void RunEnrollmentTest() {
    // Send the profile prepared notification to continue the enrollment.
    service_->Observe(chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
                      content::Source<void>(NULL),  // Not used.
                      content::Details<Profile>(testing_owner_profile_.get()));
    base::RunLoop().RunUntilIdle();
  }

  content::TestBrowserThreadBundle thread_bundle_;

  NiceMock<chromeos::MockUserManager>* mock_user_manager_;
  chromeos::ScopedUserManagerEnabler scoped_user_manager_enabler_;
  scoped_ptr<TestingProfile> testing_owner_profile_;
  FakeDeviceCloudPolicyInitializer* fake_initializer_;
  EnrollmentStatus enrollment_status_;
};

TEST_F(ConsumerManagementServiceEnrollmentTest, EnrollsSuccessfully) {
  EXPECT_FALSE(fake_initializer_->was_start_enrollment_called());

  RunEnrollmentTest();

  EXPECT_TRUE(fake_initializer_->was_start_enrollment_called());
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_SUCCESS,
            GetEnrollmentState());
}

TEST_F(ConsumerManagementServiceEnrollmentTest,
       EnrollmentStateIsResetIfCurrentUserIsOwner) {
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_OWNER_STORED,
            GetEnrollmentState());

  EXPECT_CALL(*mock_user_manager_, IsCurrentUserOwner())
      .WillOnce(Return(true));

  RunEnrollmentTest();

  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_NONE, GetEnrollmentState());
}

TEST_F(ConsumerManagementServiceEnrollmentTest, FailsToGetAccessToken) {
  // Disable auto-posting so that RunEnrollmentTest() should stop and wait for
  // the access token to be available.
  GetFakeProfileOAuth2TokenService()->
      set_auto_post_fetch_response_on_message_loop(false);

  RunEnrollmentTest();

  // The service should have a pending token request.
  OAuth2TokenService::Request* token_request =
      service_->GetTokenRequestForTesting();
  EXPECT_TRUE(token_request);

  // Tell the service that the access token is not available because of some
  // backend issue.
  service_->OnGetTokenFailure(
      token_request,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR));

  EXPECT_FALSE(fake_initializer_->was_start_enrollment_called());
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_GET_TOKEN_FAILED,
            GetEnrollmentState());
}

TEST_F(ConsumerManagementServiceEnrollmentTest, FailsToRegister) {
  EXPECT_FALSE(fake_initializer_->was_start_enrollment_called());
  fake_initializer_->set_enrollment_status(EnrollmentStatus::ForStatus(
      EnrollmentStatus::STATUS_REGISTRATION_FAILED));

  RunEnrollmentTest();

  EXPECT_TRUE(fake_initializer_->was_start_enrollment_called());
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_DM_SERVER_FAILED,
            GetEnrollmentState());
}

TEST_F(ConsumerManagementServiceEnrollmentTest, ShowsDesktopNotificationOnly) {
  SetEnrollmentState(ConsumerManagementService::ENROLLMENT_CANCELED);

  RunEnrollmentTest();

  // TODO(davidyu): Check that the desktop notification was shown.

  EXPECT_FALSE(fake_initializer_->was_start_enrollment_called());
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_NONE, GetEnrollmentState());
}

}  // namespace policy
