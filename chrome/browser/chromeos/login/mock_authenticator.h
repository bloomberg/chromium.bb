// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_

#include <string>

#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_utils.h"

class Profile;

namespace chromeos {

class LoginStatusConsumer;

class MockAuthenticator : public Authenticator {
 public:
  MockAuthenticator(LoginStatusConsumer* consumer,
                    const std::string& expected_username,
                    const std::string& expected_password)
      : Authenticator(consumer),
        expected_username_(expected_username),
        expected_password_(expected_password),
        authenticate_result_(true) {
  }

  // Returns true after posting task to UI thread to call OnLoginSuccess().
  // This is called on the FILE thread now, so we need to do this.
  virtual bool AuthenticateToLogin(Profile* profile,
                                   const std::string& username,
                                   const std::string& password) {
    EXPECT_EQ(expected_username_, username);
    EXPECT_EQ(expected_password_, password);

    if (authenticate_result_) {
      ChromeThread::PostTask(
          ChromeThread::UI, FROM_HERE,
          NewRunnableMethod(this,
                            &MockAuthenticator::OnLoginSuccess,
                            username));
    } else {
      ChromeThread::PostTask(
          ChromeThread::UI, FROM_HERE,
          NewRunnableMethod(this,
                            &MockAuthenticator::OnLoginFailure,
                            std::string("Login failed")));
    }
    return authenticate_result_;
  }

  virtual bool AuthenticateToUnlock(const std::string& username,
                                    const std::string& password) {
    return AuthenticateToLogin(NULL /* not used */, username, password);
  }

  void OnLoginSuccess(const std::string& username) {
    consumer_->OnLoginSuccess(username, std::string());
  }

  void OnLoginFailure(const std::string& data) {
      consumer_->OnLoginFailure(data);
      LOG(INFO) << "Posting a QuitTask to UI thread";
      ChromeThread::PostTask(
          ChromeThread::UI, FROM_HERE, new MessageLoop::QuitTask);
  }

  void set_authenticate_result(bool b) {
    authenticate_result_ = b;
  }

 private:
  std::string expected_username_;
  std::string expected_password_;
  bool authenticate_result_;

  DISALLOW_COPY_AND_ASSIGN(MockAuthenticator);
};

class MockLoginUtils : public LoginUtils {
 public:
  explicit MockLoginUtils(const std::string& expected_username,
                          const std::string& expected_password)
      : expected_username_(expected_username),
        expected_password_(expected_password) {
  }

  virtual void CompleteLogin(const std::string& username,
                             const std::string& cookies) {
    EXPECT_EQ(expected_username_, username);
  }

  virtual Authenticator* CreateAuthenticator(LoginStatusConsumer* consumer) {
    return new MockAuthenticator(
        consumer, expected_username_, expected_password_);
  }

 private:
  std::string expected_username_;
  std::string expected_password_;

  DISALLOW_COPY_AND_ASSIGN(MockLoginUtils);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_
