// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/signin_manager.h"

#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/net/gaia/token_service_unittest.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/sync/util/oauth.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/test/base/signaling_task.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_url_fetcher_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

#include "testing/gtest/include/gtest/gtest.h"

class SigninManagerTest : public TokenServiceTestHarness {
 public:
  virtual void SetUp() {
    TokenServiceTestHarness::SetUp();
    manager_.reset(new SigninManager());
    google_login_success_.ListenFor(
        chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
        Source<Profile>(profile_.get()));
    google_login_failure_.ListenFor(chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
                                    Source<Profile>(profile_.get()));
  }

  void SimulateValidResponseClientLogin() {
    DCHECK(!browser_sync::IsUsingOAuth());
    // Simulate the correct ClientLogin response.
    TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    DCHECK(fetcher);
    DCHECK(fetcher->delegate());
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, GURL(GaiaUrls::GetInstance()->client_login_url()),
        net::URLRequestStatus(), 200, net::ResponseCookies(),
        "SID=sid\nLSID=lsid\nAuth=auth");

    // Then simulate the correct GetUserInfo response for the canonical email.
    // A new URL fetcher is used for each call.
    fetcher = factory_.GetFetcherByID(0);
    DCHECK(fetcher);
    DCHECK(fetcher->delegate());
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, GURL(GaiaUrls::GetInstance()->get_user_info_url()),
        net::URLRequestStatus(), 200, net::ResponseCookies(),
        "email=user@gmail.com");
  }

  void SimulateValidSigninOAuth() {
    DCHECK(browser_sync::IsUsingOAuth());
    // Simulate a valid OAuth-based signin
    manager_->OnGetOAuthTokenSuccess("oauth_token-Ev1Vu1hv");
    manager_->OnOAuthGetAccessTokenSuccess("oauth1_access_token-qOAmlrSM",
                                           "secret-NKKn1DuR");
    manager_->OnOAuthWrapBridgeSuccess(browser_sync::SyncServiceName(),
                                       "oauth2_wrap_access_token-R0Z3nRtw",
                                       "3600");
    manager_->OnUserInfoSuccess("user-xZIuqTKu@gmail.com");
  }


  TestURLFetcherFactory factory_;
  scoped_ptr<SigninManager> manager_;
  TestNotificationTracker google_login_success_;
  TestNotificationTracker google_login_failure_;
};

// NOTE: ClientLogin's "StartSignin" is called after collecting credentials
//       from the user.  See also SigninManagerTest::SignInOAuth.
TEST_F(SigninManagerTest, SignInClientLogin) {
  browser_sync::SetIsUsingOAuthForTest(false);
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetUsername().empty());

  manager_->StartSignIn("username", "password", "", "");
  EXPECT_FALSE(manager_->GetUsername().empty());

  SimulateValidResponseClientLogin();

  // Should go into token service and stop.
  EXPECT_EQ(1U, google_login_success_.size());
  EXPECT_EQ(0U, google_login_failure_.size());

  // Should persist across resets.
  manager_.reset(new SigninManager());
  manager_->Initialize(profile_.get());
  EXPECT_EQ("user@gmail.com", manager_->GetUsername());
}

// NOTE: OAuth's "StartOAuthSignIn" is called before collecting credentials
//       from the user.  See also SigninManagerTest::SignInClientLogin.
TEST_F(SigninManagerTest, SignInOAuth) {
  browser_sync::SetIsUsingOAuthForTest(true);
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetUsername().empty());

  SimulateValidSigninOAuth();
  EXPECT_FALSE(manager_->GetUsername().empty());

  // Should go into token service and stop.
  EXPECT_EQ(1U, google_login_success_.size());
  EXPECT_EQ(0U, google_login_failure_.size());

  // Should persist across resets.
  manager_.reset(new SigninManager());
  manager_->Initialize(profile_.get());
  EXPECT_EQ("user-xZIuqTKu@gmail.com", manager_->GetUsername());
}

TEST_F(SigninManagerTest, SignOutClientLogin) {
  browser_sync::SetIsUsingOAuthForTest(false);
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", "", "");
  SimulateValidResponseClientLogin();
  manager_->OnClientLoginSuccess(credentials_);

  EXPECT_EQ("user@gmail.com", manager_->GetUsername());
  manager_->SignOut();
  EXPECT_TRUE(manager_->GetUsername().empty());
  // Should not be persisted anymore
  manager_.reset(new SigninManager());
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetUsername().empty());
}

TEST_F(SigninManagerTest, SignOutOAuth) {
  browser_sync::SetIsUsingOAuthForTest(true);
  manager_->Initialize(profile_.get());

  SimulateValidSigninOAuth();
  EXPECT_FALSE(manager_->GetUsername().empty());

  EXPECT_EQ("user-xZIuqTKu@gmail.com", manager_->GetUsername());
  manager_->SignOut();
  EXPECT_TRUE(manager_->GetUsername().empty());

  // Should not be persisted anymore
  manager_.reset(new SigninManager());
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetUsername().empty());
}

TEST_F(SigninManagerTest, SignInFailureClientLogin) {
  browser_sync::SetIsUsingOAuthForTest(false);
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", "", "");
  GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
  manager_->OnClientLoginFailure(error);

  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(1U, google_login_failure_.size());

  EXPECT_TRUE(manager_->GetUsername().empty());

  // Should not be persisted
  manager_.reset(new SigninManager());
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetUsername().empty());
}

TEST_F(SigninManagerTest, ProvideSecondFactorSuccess) {
  browser_sync::SetIsUsingOAuthForTest(false);
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", "", "");
  GoogleServiceAuthError error(GoogleServiceAuthError::TWO_FACTOR);
  manager_->OnClientLoginFailure(error);

  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(1U, google_login_failure_.size());

  EXPECT_FALSE(manager_->GetUsername().empty());

  manager_->ProvideSecondFactorAccessCode("access");
  SimulateValidResponseClientLogin();

  EXPECT_EQ(1U, google_login_success_.size());
  EXPECT_EQ(1U, google_login_failure_.size());
}

TEST_F(SigninManagerTest, ProvideSecondFactorFailure) {
  browser_sync::SetIsUsingOAuthForTest(false);
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", "", "");
  GoogleServiceAuthError error1(GoogleServiceAuthError::TWO_FACTOR);
  manager_->OnClientLoginFailure(error1);

  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(1U, google_login_failure_.size());

  EXPECT_FALSE(manager_->GetUsername().empty());

  manager_->ProvideSecondFactorAccessCode("badaccess");
  GoogleServiceAuthError error2(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  manager_->OnClientLoginFailure(error2);

  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(2U, google_login_failure_.size());
  EXPECT_FALSE(manager_->GetUsername().empty());

  manager_->ProvideSecondFactorAccessCode("badaccess");
  GoogleServiceAuthError error3(GoogleServiceAuthError::CONNECTION_FAILED);
  manager_->OnClientLoginFailure(error3);

  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(3U, google_login_failure_.size());
  EXPECT_TRUE(manager_->GetUsername().empty());
}

TEST_F(SigninManagerTest, SignOutMidConnect) {
  browser_sync::SetIsUsingOAuthForTest(false);
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", "", "");
  manager_->SignOut();
  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(0U, google_login_failure_.size());

  EXPECT_TRUE(manager_->GetUsername().empty());
}
