// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/policy/mock_cloud_policy_store.h"
#include "chrome/browser/policy/user_cloud_policy_manager.h"
#include "chrome/browser/policy/user_policy_signin_service.h"
#include "chrome/browser/policy/user_policy_signin_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_manager_fake.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace policy {

namespace {

class UserPolicySigninServiceTest : public testing::Test {
 public:
  UserPolicySigninServiceTest()
      : loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &loop_) {
  }

  virtual void SetUp() OVERRIDE {
    local_state_.reset(new TestingPrefService);
    chrome::RegisterLocalState(local_state_.get());
    static_cast<TestingBrowserProcess*>(g_browser_process)->SetLocalState(
        local_state_.get());

    // Create a UserCloudPolicyManager with a MockCloudPolicyStore, and build a
    // TestingProfile that uses it.
    mock_store_ = new MockCloudPolicyStore();
    mock_store_->NotifyStoreLoaded();
    EXPECT_CALL(*mock_store_, Load());
    scoped_ptr<UserCloudPolicyManager> manager(new UserCloudPolicyManager(
        scoped_ptr<CloudPolicyStore>(mock_store_), false));
    TestingProfile::Builder builder;
    builder.SetUserCloudPolicyManager(manager.Pass());
    profile_ = builder.Build().Pass();
    profile_->GetPrefs()->SetBoolean(prefs::kLoadCloudPolicyOnSignin, true);
    SigninManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(), FakeSigninManager::Build);

    // Make sure the UserPolicySigninService is created.
    UserPolicySigninServiceFactory::GetForProfile(profile_.get());
  }

  virtual void TearDown() OVERRIDE {
    // Free the profile before we clear out the browser prefs.
    profile_.reset();
    static_cast<TestingBrowserProcess*>(g_browser_process)->SetLocalState(NULL);
    local_state_.reset();
  }


  scoped_ptr<TestingProfile> profile_;
  // Weak pointer to a MockCloudPolicyStore - lifetime is managed by the
  // UserCloudPolicyManager.
  MockCloudPolicyStore* mock_store_;

  // BrowserPolicyConnector wants to initialize various components
  // asynchronously via tasks, so create a fake thread here.
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;

  scoped_ptr<TestingPrefService> local_state_;
};

TEST_F(UserPolicySigninServiceTest, InitWhileSignedOut) {
  // Make sure user is not signed in.
  ASSERT_TRUE(SigninManagerFactory::GetForProfile(profile_.get())->
         GetAuthenticatedUsername().empty());

  // Let the SigninService know that the profile has been created.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());

  // UserCloudPolicyManager should not be initialized.
  ASSERT_FALSE(profile_->GetUserCloudPolicyManager()->cloud_policy_service());
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
  ASSERT_TRUE(profile_->GetUserCloudPolicyManager()->cloud_policy_service());
}

// TODO(atwilson): Enable test for signing in once it is possible to use a
// mock TokenService (http://crbug.com/138618).
TEST_F(UserPolicySigninServiceTest, DISABLED_SignInAfterInit) {
  // Let the SigninService know that the profile has been created.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());

  // UserCloudPolicyManager should not be initialized.
  ASSERT_FALSE(profile_->GetUserCloudPolicyManager()->cloud_policy_service());

  // Now sign in the user.
  SigninManagerFactory::GetForProfile(profile_.get())->SetAuthenticatedUsername(
      "testuser@test.com");

  // Make oauth token available (needs MockTokenService - see TODO above).

  // UserCloudPolicyManager should be initialized.
  ASSERT_TRUE(profile_->GetUserCloudPolicyManager()->cloud_policy_service());
}

TEST_F(UserPolicySigninServiceTest, SignOutAfterInit) {
  // Set the user as signed in.
  SigninManagerFactory::GetForProfile(profile_.get())->SetAuthenticatedUsername(
      "testuser@test.com");

  // Let the SigninService know that the profile has been created.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(profile_.get()),
      content::NotificationService::NoDetails());

  // UserCloudPolicyManager should be initialized.
  ASSERT_TRUE(profile_->GetUserCloudPolicyManager()->cloud_policy_service());

  // Now sign out.
  SigninManagerFactory::GetForProfile(profile_.get())->SignOut();

  // UserCloudPolicyManager should be shut down.
  ASSERT_FALSE(profile_->GetUserCloudPolicyManager()->cloud_policy_service());
}

}  // namespace

}  // namespace policy
