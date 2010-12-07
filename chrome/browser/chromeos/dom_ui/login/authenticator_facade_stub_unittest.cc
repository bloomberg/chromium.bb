// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/dom_ui/login/authenticator_facade_stub.h"
#include "chrome/browser/chromeos/dom_ui/login/mock_login_ui_handler.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

class LoginStatusConsumer;

namespace {

class AuthenticatorFacadeStubHarness
    : public chromeos::AuthenticatorFacadeStub {
 public:
  AuthenticatorFacadeStubHarness(chromeos::LoginStatusConsumer* consumer,
                                 const std::string& username,
                                 const std::string& password) :
      chromeos::AuthenticatorFacadeStub(consumer,
                                        username,
                                        password) {}
  const std::string GetUsername() const { return expected_username_; }
  const std::string GetPassword() const { return expected_password_; }
  chromeos::LoginStatusConsumer* GetConsumer() const { return consumer_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorFacadeStubHarness);
};

}  // namespace

namespace chromeos {

// Setup Tests
TEST(AuthenticatorFacadeStubTest, SetupSuccess) {
  MockLoginUIHandler consumer;
  std::string test_username("Test_User");
  std::string test_password("Test_Password");
  scoped_ptr<AuthenticatorFacadeStubHarness>
      facade(new AuthenticatorFacadeStubHarness(&consumer,
                                                test_username,
                                                test_password));
  ASSERT_EQ(&consumer, facade->GetConsumer());
  ASSERT_EQ(test_username, facade->GetUsername());
  ASSERT_EQ(test_password, facade->GetPassword());
}

// AuthenticateToLogin Tests
TEST(AuthenticatorFacadeStubTest, AuthenticateToLoginSuccess) {
  MockLoginUIHandler consumer;
  std::string test_username("Test_User");
  std::string test_password("Test_Password");
  std::string test_token("Test_Token");
  std::string test_captcha("Test_Captcha");
  scoped_ptr<AuthenticatorFacadeStubHarness>
      facade(new AuthenticatorFacadeStubHarness(&consumer,
                                                test_username,
                                                test_password));

  EXPECT_CALL(consumer, OnLoginSuccess(test_username,
                                       _,
                                       _,
                                       false))
      .Times(1);

  facade->AuthenticateToLogin(NULL,
                              test_username,
                              test_password,
                              test_token,
                              test_captcha);
}

TEST(AuthenticatorFacadeStubTest, AuthenticateToLoginFailure) {
  MockLoginUIHandler consumer;
  std::string test_username("Test_User");
  std::string test_password("Test_Password");
  std::string bad_username("Bad_User");
  std::string bad_password("Bad_Password");
  std::string test_token("Test_Token");
  std::string test_captcha("Test_Captcha");
  scoped_ptr<AuthenticatorFacadeStubHarness>
      facade(new AuthenticatorFacadeStubHarness(&consumer,
                                                test_username,
                                                test_password));
  EXPECT_CALL(consumer, OnLoginFailure(_))
      .Times(1);

  facade->AuthenticateToLogin(NULL,
                              bad_username,
                              test_password,
                              test_token,
                              test_captcha);

  EXPECT_CALL(consumer, OnLoginFailure(_))
      .Times(1);

  facade->AuthenticateToLogin(NULL,
                              test_username,
                              bad_password,
                              test_token,
                              test_captcha);

  EXPECT_CALL(consumer, OnLoginFailure(_))
      .Times(1);

  facade->AuthenticateToLogin(NULL,
                              bad_username,
                              bad_password,
                              test_token,
                              test_captcha);
}

// AuthenticateToUnlock
TEST(AuthenticatorFacadeStubTest, AuthenticateToUnlockSuccess) {
  MockLoginUIHandler consumer;
  std::string test_username("Test_User");
  std::string test_password("Test_Password");
  scoped_ptr<AuthenticatorFacadeStubHarness>
      facade(new AuthenticatorFacadeStubHarness(&consumer,
                                                test_username,
                                                test_password));

  EXPECT_CALL(consumer, OnLoginSuccess(test_username,
                                       _,
                                       _,
                                       false))
      .Times(1);

  facade->AuthenticateToUnlock(test_username, test_password);
}

TEST(AuthenticatorFacadeStubTest, AuthenticateToUnlockFailure) {
  MockLoginUIHandler consumer;
  std::string test_username("Test_User");
  std::string test_password("Test_Password");
  std::string bad_username("Bad_User");
  std::string bad_password("Bad_Password");
  scoped_ptr<AuthenticatorFacadeStubHarness>
      facade(new AuthenticatorFacadeStubHarness(&consumer,
                                                test_username,
                                                test_password));
  EXPECT_CALL(consumer, OnLoginFailure(_))
      .Times(1);

  facade->AuthenticateToUnlock(bad_username, test_password);

  EXPECT_CALL(consumer, OnLoginFailure(_))
      .Times(1);

  facade->AuthenticateToUnlock(test_username, bad_password);

  EXPECT_CALL(consumer, OnLoginFailure(_))
      .Times(1);

  facade->AuthenticateToUnlock(bad_username, bad_password);
}

}  // namespace chromeos
