// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/policy/browser_policy_connector.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyNumber;
using testing::Mock;

namespace policy {

namespace {

class UserPolicySigninServiceTest : public testing::Test {
 public:
  UserPolicySigninServiceTest()
      : loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &loop_),
        file_thread_(content::BrowserThread::FILE, &loop_),
        io_thread_(content::BrowserThread::IO, &loop_) {}

  virtual void SetUp() OVERRIDE {
    g_browser_process->browser_policy_connector()->Init();

    local_state_.reset(new TestingPrefService);
    chrome::RegisterLocalState(local_state_.get());
    static_cast<TestingBrowserProcess*>(g_browser_process)->SetLocalState(
        local_state_.get());

    // Create a testing profile and bring up a UserCloudPolicyManager with a
    // MockUserCloudPolicyStore.
    profile_.reset(new TestingProfile());
    profile_->CreateRequestContext();
    profile_->GetPrefs()->SetBoolean(prefs::kLoadCloudPolicyOnSignin, true);

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
  ASSERT_FALSE(manager_->cloud_policy_service());
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
  ASSERT_TRUE(manager_->cloud_policy_service());

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
  ASSERT_FALSE(manager_->cloud_policy_service());

  // Now sign in the user.
  SigninManagerFactory::GetForProfile(profile_.get())->SetAuthenticatedUsername(
      "testuser@test.com");

  // Complete initialization of the store.
  mock_store_->NotifyStoreLoaded();

  // Make oauth token available.
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, "oauth_login_refresh_token");

  // UserCloudPolicyManager should be initialized.
  ASSERT_TRUE(manager_->cloud_policy_service());

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
  ASSERT_FALSE(manager_->cloud_policy_service());

  // Now sign in the user.
  SigninManagerFactory::GetForProfile(profile_.get())->SetAuthenticatedUsername(
      "testuser@test.com");

  // Make oauth token available.
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, "oauth_login_refresh_token");

  // UserCloudPolicyManager should be initialized.
  ASSERT_TRUE(manager_->cloud_policy_service());

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
  ASSERT_FALSE(manager_->cloud_policy_service());

  // Now sign in the user.
  SigninManagerFactory::GetForProfile(profile_.get())->SetAuthenticatedUsername(
      "testuser@test.com");

  // Make oauth token available.
  TokenServiceFactory::GetForProfile(profile_.get())->IssueAuthTokenForTest(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, "oauth_login_refresh_token");

  // UserCloudPolicyManager should be initialized.
  ASSERT_TRUE(manager_->cloud_policy_service());

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
  ASSERT_TRUE(manager_->cloud_policy_service());

  // Now sign out.
  SigninManagerFactory::GetForProfile(profile_.get())->SignOut();

  // UserCloudPolicyManager should be shut down.
  ASSERT_FALSE(manager_->cloud_policy_service());
}

}  // namespace

}  // namespace policy
