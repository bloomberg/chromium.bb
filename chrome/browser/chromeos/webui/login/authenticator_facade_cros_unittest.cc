// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/webui/login/authenticator_facade_cros.h"
#include "chrome/browser/chromeos/webui/login/mock_authenticator_facade_cros_helpers.h"
#include "chrome/browser/chromeos/webui/login/mock_login_ui_handler.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace {

class AuthenticatorFacadeCrosHarness
    : public chromeos::AuthenticatorFacadeCros {
 public:
  AuthenticatorFacadeCrosHarness(
      chromeos::LoginStatusConsumer* consumer)
      : chromeos::AuthenticatorFacadeCros(consumer) {
  }

  chromeos::LoginStatusConsumer* GetConsumer() const {
    return consumer_;
  }
  chromeos::MockAuthenticatorFacadeCrosHelpers* GetHelpers() const {
    return static_cast<chromeos::MockAuthenticatorFacadeCrosHelpers*>(
        helpers_.get());
  }
  void SetHelpers(chromeos::MockAuthenticatorFacadeCrosHelpers* helpers) {
    helpers_.reset(helpers);
  }
  chromeos::MockAuthenticator* GetAuthenticator() const {
    return static_cast<chromeos::MockAuthenticator*>(
        authenticator_.get());
  }
  void SetAuthenticator(chromeos::MockAuthenticator* authenticator) {
    authenticator_ = authenticator;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorFacadeCrosHarness);
};

}  // namespace

namespace chromeos {

// Setup Tests
TEST(AuthenticatorFacadeCrosTest, Setup) {
  MockLoginUIHandler consumer;
  MockAuthenticatorFacadeCrosHelpers* helpers =
      new chromeos::MockAuthenticatorFacadeCrosHelpers();
  scoped_refptr<chromeos::MockAuthenticator> authenticator =
      new chromeos::MockAuthenticator(&consumer,
                                      std::string(),
                                      std::string());
  TestingProfile mock_profile;
  AuthenticatorFacadeCrosHarness facade(&consumer);

  facade.SetHelpers(helpers);
  EXPECT_CALL(*helpers, CreateAuthenticator(&consumer))
      .Times(1)
      .WillRepeatedly(Return(authenticator.get()));

  facade.Setup();

  ASSERT_EQ(&consumer, facade.GetConsumer());
  ASSERT_EQ(authenticator.get(), facade.GetAuthenticator());
}

// AuthenticateToLogin Tests
TEST(AuthenticatorFacadeCrosTest, AuthenticateToLogin) {
  MockLoginUIHandler consumer;
  MockAuthenticatorFacadeCrosHelpers* helpers =
      new chromeos::MockAuthenticatorFacadeCrosHelpers();
  scoped_refptr<chromeos::MockAuthenticator> authenticator =
      new chromeos::MockAuthenticator(&consumer,
                                      std::string(),
                                      std::string());
  TestingProfile mock_profile;
  std::string test_username("Test_User");
  std::string test_password("Test_Password");
  std::string test_token("Test_Token");
  std::string test_captcha("Test_Captcha");
  AuthenticatorFacadeCrosHarness facade(&consumer);

  facade.SetHelpers(helpers);
  facade.SetAuthenticator(authenticator.get());
  EXPECT_CALL(*helpers, PostAuthenticateToLogin(authenticator.get(),
                                                &mock_profile,
                                                test_username,
                                                test_password))
      .Times(1);

  facade.AuthenticateToLogin(&mock_profile,
                              test_username,
                              test_password,
                              test_token,
                              test_captcha);
}

// AuthenticateToUnlock
TEST(AuthenticatorFacadeCrosTest, AuthenticateToUnlock) {
  MockLoginUIHandler consumer;
  MockAuthenticatorFacadeCrosHelpers* helpers =
      new chromeos::MockAuthenticatorFacadeCrosHelpers();
  scoped_refptr<chromeos::MockAuthenticator> authenticator =
      new chromeos::MockAuthenticator(&consumer,
                                      std::string(),
                                      std::string());
  std::string test_username("Test_User");
  std::string test_password("Test_Password");
  AuthenticatorFacadeCrosHarness facade(&consumer);

  facade.SetHelpers(helpers);
  facade.SetAuthenticator(authenticator.get());
  EXPECT_CALL(*helpers, PostAuthenticateToLogin(authenticator.get(),
                                                NULL,
                                                test_username,
                                                test_password))
      .Times(1);

  facade.AuthenticateToUnlock(test_username, test_password);
}

}  // namespace chromeos
