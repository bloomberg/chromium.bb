// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_

#include <string>

#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

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
        expected_password_(expected_password) {
  }

  // Returns true after posting task to UI thread to call OnLoginSuccess().
  // This is called on the FILE thread now, so we need to do this.
  virtual bool AuthenticateToLogin(Profile* profile,
                                   const std::string& username,
                                   const std::string& password) {
    if (expected_username_ == username &&
        expected_password_ == password) {
      ChromeThread::PostTask(
          ChromeThread::UI, FROM_HERE,
          NewRunnableMethod(this,
                            &MockAuthenticator::OnLoginSuccess,
                            username));
      return true;
    } else {
      ChromeThread::PostTask(
          ChromeThread::UI, FROM_HERE,
          NewRunnableMethod(this,
                            &MockAuthenticator::OnLoginFailure,
                            std::string("Login failed")));
      return false;
    }
  }

  virtual bool AuthenticateToUnlock(const std::string& username,
                                    const std::string& password) {
    return AuthenticateToLogin(NULL /* not used */, username, password);
  }

  virtual void LoginOffTheRecord() {
    consumer_->OnOffTheRecordLoginSuccess();
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

 private:
  std::string expected_username_;
  std::string expected_password_;

  DISALLOW_COPY_AND_ASSIGN(MockAuthenticator);
};

class MockLoginUtils : public LoginUtils {
 public:
  explicit MockLoginUtils(const std::string& expected_username,
                          const std::string& expected_password)
      : expected_username_(expected_username),
        expected_password_(expected_password) {
  }

  virtual bool ShouldWaitForWifi() {
    return false;
  }

  virtual void CompleteLogin(const std::string& username,
                             const std::string& cookies) {
    EXPECT_EQ(expected_username_, username);
  }

  virtual void CompleteOffTheRecordLogin() {
  }

  virtual Authenticator* CreateAuthenticator(LoginStatusConsumer* consumer) {
    return new MockAuthenticator(
        consumer, expected_username_, expected_password_);
  }

  virtual void EnableBrowserLaunch(bool enable) {
  }

  virtual bool IsBrowserLaunchEnabled() const {
    return true;
  }

  virtual const std::string& GetAuthToken() const {
    return auth_token_;
  }

 private:
  std::string expected_username_;
  std::string expected_password_;
  std::string auth_token_;

  DISALLOW_COPY_AND_ASSIGN(MockLoginUtils);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_
