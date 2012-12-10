// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/mock_device_management_service.h"
#include "chrome/browser/policy/mock_user_cloud_policy_store.h"
#include "chrome/browser/policy/user_cloud_policy_manager.h"
#include "chrome/browser/policy/user_policy_signin_service.h"
#include "chrome/browser/policy/user_policy_signin_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_manager_fake.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::AnyNumber;
using testing::Mock;
using testing::_;

namespace policy {

namespace {

class UserPolicySigninServiceTest : public testing::Test {
 public:
  UserPolicySigninServiceTest()
      : loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &loop_),
        file_thread_(content::BrowserThread::FILE, &loop_),
        io_thread_(content::BrowserThread::IO, &loop_) {}

  MOCK_METHOD1(OnPolicyRefresh, void(bool));

  virtual void SetUp() OVERRIDE {
    device_management_service_ = new MockDeviceManagementService();
    g_browser_process->browser_policy_connector()->
        SetDeviceManagementServiceForTesting(
            scoped_ptr<DeviceManagementService>(device_management_service_));

    g_browser_process->browser_policy_connector()->Init();

    local_state_.reset(new TestingPrefService);
    chrome::RegisterLocalState(local_state_.get());
    static_cast<TestingBrowserProcess*>(g_browser_process)->SetLocalState(
        local_state_.get());

    // Create a testing profile with cloud-policy-on-signin enabled, and bring
    // up a UserCloudPolicyManager with a MockUserCloudPolicyStore.
    scoped_ptr<TestingPrefService> prefs(new TestingPrefService());
    Profile::RegisterUserPrefs(prefs.get());
    chrome::RegisterUserPrefs(prefs.get());
    prefs->SetUserPref(prefs::kLoadCloudPolicyOnSignin,
                       Value::CreateBooleanValue(true));
    TestingProfile::Builder builder;
    builder.SetPrefService(scoped_ptr<PrefService>(prefs.Pass()));
    profile_ = builder.Build().Pass();
    profile_->CreateRequestContext();

    mock_store_ = new MockUserCloudPolicyStore();
    EXPECT_CALL(*mock_store_, Load()).Times(AnyNumber());
    manager_.reset(new UserCloudPolicyManager(
        profile_.get(), scoped_ptr<UserCloudPolicyStore>(mock_store_)));
    SigninManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(), FakeSigninManager::Build);

    // Make sure the UserPolicySigninService is created.
    UserPolicySigninServiceFactory::GetForProfile(profile_.get());
    Mock::VerifyAndClearExpectations(mock_store_);
  }

  virtual void TearDown() OVERRIDE {
    // Free the profile before we clear out the browser prefs.
    profile_.reset();
    TestingBrowserProcess* testing_browser_process =
        static_cast<TestingBrowserProcess*>(g_browser_process);
    testing_browser_process->SetLocalState(NULL);
    local_state_.reset();
    testing_browser_process->SetBrowserPolicyConnector(NULL);
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  bool IsRequestActive() {
    return url_factory_.GetFetcherByID(0);
  }

  scoped_ptr<TestingProfile> profile_;
  // Weak pointer to a MockUserCloudPolicyStore - lifetime is managed by the
  // UserCloudPolicyManager.
  MockUserCloudPolicyStore* mock_store_;
  scoped_ptr<UserCloudPolicyManager> manager_;

  // BrowserPolicyConnector and UrlFetcherFactory want to initialize and free
  // various components asynchronously via tasks, so create fake threads here.
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;

  net::TestURLFetcherFactory url_factory_;

  // Weak ptr to the MockDeviceManagementService (object is owned by the
  // BrowserPolicyConnector).
  MockDeviceManagementService* device_management_service_;

  scoped_ptr<TestingPrefService> local_state_;
};

TEST_F(UserPolicySigninServiceTest, InitWhileSignedOut) {
  EXPECT_CALL(*mock_store_, Clear());
  // Make sure user is not signed in.
  ASSERT_TRUE(SigninManagerFactory::GetForProfile(profile_.get())->
      GetAuthenticatedUsername().empty());

  // Let the SigninService know that the profile has been created.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());

  // UserCloudPolicyManager should not be initialized.
  ASSERT_FALSE(manager_->core()->service());
}

TEST_F(UserPolicySigninServiceTest, InitWhileSignedIn) {
  // Set the user as signed in.
  SigninManagerFactory::GetForProfile(profile_.get())->SetAuthenticatedUsername(
      "testuser@test.com");

  // Let the SigninService know that the profile has been created.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());

  // UserCloudPolicyManager should be initialized.
  ASSERT_TRUE(manager_->core()->service());

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // No oauth access token yet, so client registration should be deferred.
  ASSERT_FALSE(IsRequestActive());

  // Make oauth token available.
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, "oauth_login_refresh_token");

  // Client registration should be in progress since we now have an oauth token.
  ASSERT_TRUE(IsRequestActive());
}

TEST_F(UserPolicySigninServiceTest, SignInAfterInit) {
  EXPECT_CALL(*mock_store_, Clear());
  // Let the SigninService know that the profile has been created.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());

  // UserCloudPolicyManager should not be initialized since there is no
  // signed-in user.
  ASSERT_FALSE(manager_->core()->service());

  // Now sign in the user.
  SigninManagerFactory::GetForProfile(profile_.get())->SetAuthenticatedUsername(
      "testuser@test.com");

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // Make oauth token available.
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, "oauth_login_refresh_token");

  // UserCloudPolicyManager should be initialized.
  ASSERT_TRUE(manager_->core()->service());

  // Client registration should be in progress since we have an oauth token.
  ASSERT_TRUE(IsRequestActive());
}

TEST_F(UserPolicySigninServiceTest, UnregisteredClient) {
  EXPECT_CALL(*mock_store_, Clear());
  // Let the SigninService know that the profile has been created.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());

  // UserCloudPolicyManager should not be initialized since there is no
  // signed-in user.
  ASSERT_FALSE(manager_->core()->service());

  // Now sign in the user.
  SigninManagerFactory::GetForProfile(profile_.get())->SetAuthenticatedUsername(
      "testuser@test.com");

  // Make oauth token available.
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, "oauth_login_refresh_token");

  // UserCloudPolicyManager should be initialized.
  ASSERT_TRUE(manager_->core()->service());

  // Client registration should not be in progress since the store is not
  // yet initialized.
  ASSERT_FALSE(IsRequestActive());

  // Complete initialization of the store with no policy (unregistered client).
  mock_store_->NotifyStoreLoaded();

  // Client registration should be in progress since we have an oauth token.
  ASSERT_TRUE(IsRequestActive());
}

TEST_F(UserPolicySigninServiceTest, RegisteredClient) {
  EXPECT_CALL(*mock_store_, Clear());
  // Let the SigninService know that the profile has been created.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());

  // UserCloudPolicyManager should not be initialized since there is no
  // signed-in user.
  ASSERT_FALSE(manager_->core()->service());

  // Now sign in the user.
  SigninManagerFactory::GetForProfile(profile_.get())->SetAuthenticatedUsername(
      "testuser@test.com");

  // Make oauth token available.
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, "oauth_login_refresh_token");

  // UserCloudPolicyManager should be initialized.
  ASSERT_TRUE(manager_->core()->service());

  // Client registration should not be in progress since the store is not
  // yet initialized.
  ASSERT_FALSE(manager_->IsClientRegistered());
  ASSERT_FALSE(IsRequestActive());

  mock_store_->policy_.reset(new enterprise_management::PolicyData());
  mock_store_->policy_->set_request_token("fake token");
  mock_store_->policy_->set_device_id("fake client id");

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // Client registration should not be in progress since the client should be
  // already registered.
  ASSERT_TRUE(manager_->IsClientRegistered());
  ASSERT_FALSE(IsRequestActive());
}

TEST_F(UserPolicySigninServiceTest, SignOutAfterInit) {
  EXPECT_CALL(*mock_store_, Clear());
  // Set the user as signed in.
  SigninManagerFactory::GetForProfile(profile_.get())->SetAuthenticatedUsername(
      "testuser@test.com");

  // Let the SigninService know that the profile has been created.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());

  // UserCloudPolicyManager should be initialized.
  ASSERT_TRUE(manager_->core()->service());

  // Now sign out.
  SigninManagerFactory::GetForProfile(profile_.get())->SignOut();

  // UserCloudPolicyManager should be shut down.
  ASSERT_FALSE(manager_->core()->service());
}

TEST_F(UserPolicySigninServiceTest, FetchPolicyFailure) {
  // Set the user as signed in.
  SigninManagerFactory::GetForProfile(profile_.get())->SetAuthenticatedUsername(
      "testuser@test.com");

  UserPolicySigninService* signin_service =
      UserPolicySigninServiceFactory::GetForProfile(profile_.get());
  EXPECT_CALL(*this, OnPolicyRefresh(_)).Times(0);
  signin_service->FetchPolicyForSignedInUser(
      "mock_token",
      base::Bind(&UserPolicySigninServiceTest::OnPolicyRefresh,
                 base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);

  // UserCloudPolicyManager should be initialized.
  ASSERT_TRUE(manager_->core()->service());
  ASSERT_TRUE(IsRequestActive());

  // Cause the fetch to fail - callback should be invoked.
  EXPECT_CALL(*this, OnPolicyRefresh(false)).Times(1);
  net::TestURLFetcher* fetcher = url_factory_.GetFetcherByID(0);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::FAILED, -1));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(UserPolicySigninServiceTest, FetchPolicySuccess) {
  // Set the user as signed in.
  SigninManagerFactory::GetForProfile(profile_.get())->SetAuthenticatedUsername(
      "testuser@test.com");

  UserPolicySigninService* signin_service =
      UserPolicySigninServiceFactory::GetForProfile(profile_.get());
  EXPECT_CALL(*this, OnPolicyRefresh(true)).Times(0);
  signin_service->FetchPolicyForSignedInUser(
      "mock_token",
      base::Bind(&UserPolicySigninServiceTest::OnPolicyRefresh,
                 base::Unretained(this)));

  // UserCloudPolicyManager should be initialized.
  ASSERT_TRUE(manager_->core()->service());
  ASSERT_TRUE(IsRequestActive());

  // Mimic successful client registration - this should kick off a policy
  // fetch.
  MockDeviceManagementJob* fetch_request = NULL;
  EXPECT_CALL(*device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH))
      .WillOnce(device_management_service_->CreateAsyncJob(&fetch_request));
  EXPECT_CALL(*device_management_service_, StartJob(_, _, _, _, _, _, _))
        .Times(1);
  manager_->core()->client()->SetupRegistration("dm_token", "client_id");
  ASSERT_TRUE(fetch_request);
  Mock::VerifyAndClearExpectations(this);

  // Make the policy fetch succeed - this should result in a write to the
  // store and ultimately result in a call to OnPolicyRefresh().
  EXPECT_CALL(*mock_store_, Store(_));
  EXPECT_CALL(*this, OnPolicyRefresh(true)).Times(1);
  // Create a fake policy blob to deliver to the client.
  em::DeviceManagementResponse policy_blob;
  em::PolicyData policy_data;
  em::PolicyFetchResponse* policy_response =
      policy_blob.mutable_policy_response()->add_response();
  ASSERT_TRUE(policy_data.SerializeToString(
      policy_response->mutable_policy_data()));
  fetch_request->SendResponse(DM_STATUS_SUCCESS, policy_blob);

  // Complete the store which should cause the policy fetch callback to be
  // invoked.
  mock_store_->NotifyStoreLoaded();
}

}  // namespace

}  // namespace policy
