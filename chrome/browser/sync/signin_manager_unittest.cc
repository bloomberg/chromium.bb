// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/signin_manager.h"

#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/net/gaia/token_service_unittest.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "chrome/test/testing_profile.h"
#include "chrome/test/signaling_task.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

#include "testing/gtest/include/gtest/gtest.h"

class SigninManagerTest : public TokenServiceTestHarness {
 public:
  virtual void SetUp() {
    TokenServiceTestHarness::SetUp();
    manager_.reset(new SigninManager());
    google_login_success_.ListenFor(NotificationType::GOOGLE_SIGNIN_SUCCESSFUL,
      Source<SigninManager>(manager_.get()));
    google_login_failure_.ListenFor(NotificationType::GOOGLE_SIGNIN_FAILED,
      Source<SigninManager>(manager_.get()));

    URLFetcher::set_factory(&factory_);
  }

  void SimulateValidResponse() {
    // Simulate the correct ClientLogin response.
    TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    DCHECK(fetcher);
    DCHECK(fetcher->delegate());
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, GURL(GaiaAuthenticator2::kClientLoginUrl), URLRequestStatus(),
        200, ResponseCookies(), "SID=sid\nLSID=lsid\nAuth=auth");

    // Then simulate the correct GetUserInfo response for the canonical email.
    // A new URL fetcher is used for each call.
    fetcher = factory_.GetFetcherByID(0);
    DCHECK(fetcher);
    DCHECK(fetcher->delegate());
    fetcher->delegate()->OnURLFetchComplete(
        fetcher, GURL(GaiaAuthenticator2::kGetUserInfoUrl), URLRequestStatus(),
        200, ResponseCookies(), "email=user@gmail.com");
  }


  TestURLFetcherFactory factory_;
  scoped_ptr<SigninManager> manager_;
  TestNotificationTracker google_login_success_;
  TestNotificationTracker google_login_failure_;
};

TEST_F(SigninManagerTest, SignIn) {
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetUsername().empty());

  manager_->StartSignIn("username", "password", "", "");
  EXPECT_FALSE(manager_->GetUsername().empty());

  SimulateValidResponse();

  // Should go into token service and stop.
  EXPECT_EQ(1U, google_login_success_.size());
  EXPECT_EQ(0U, google_login_failure_.size());

  // Should persist across resets.
  manager_.reset(new SigninManager());
  manager_->Initialize(profile_.get());
  EXPECT_EQ("user@gmail.com", manager_->GetUsername());
}

TEST_F(SigninManagerTest, SignOut) {
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", "", "");
  SimulateValidResponse();
  manager_->OnClientLoginSuccess(credentials_);

  EXPECT_EQ("user@gmail.com", manager_->GetUsername());
  manager_->SignOut();
  EXPECT_TRUE(manager_->GetUsername().empty());
  // Should not be persisted anymore
  manager_.reset(new SigninManager());
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->GetUsername().empty());
}

TEST_F(SigninManagerTest, SignInFailure) {
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

TEST_F(SigninManagerTest, SignOutMidConnect) {
  manager_->Initialize(profile_.get());
  manager_->StartSignIn("username", "password", "", "");
  manager_->SignOut();
  EXPECT_EQ(0U, google_login_success_.size());
  EXPECT_EQ(0U, google_login_failure_.size());

  EXPECT_TRUE(manager_->GetUsername().empty());
}
