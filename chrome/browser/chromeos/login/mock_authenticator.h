// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/browser/browser_thread.h"
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
                                   const std::string& password,
                                   const std::string& login_token,
                                   const std::string& login_captcha) {
    if (expected_username_ == username && expected_password_ == password) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this, &MockAuthenticator::OnLoginSuccess,
                            GaiaAuthConsumer::ClientLoginResult(), false));
      return true;
    }
    GoogleServiceAuthError error(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &MockAuthenticator::OnLoginFailure,
                          LoginFailure::FromNetworkAuthFailure(error)));
    return false;
  }

  virtual bool AuthenticateToUnlock(const std::string& username,
                                    const std::string& password) {
    return AuthenticateToLogin(NULL /* not used */, username, password,
                               std::string(), std::string());
  }

  virtual void LoginOffTheRecord() {
    consumer_->OnOffTheRecordLoginSuccess();
  }

  void OnLoginSuccess(const GaiaAuthConsumer::ClientLoginResult& credentials,
                      bool request_pending) {
    // If we want to be more like the real thing, we could save username
    // in AuthenticateToLogin, but there's not much of a point.
    consumer_->OnLoginSuccess(expected_username_,
                              expected_password_,
                              credentials,
                              request_pending);
  }

  void OnLoginFailure(const LoginFailure& failure) {
      consumer_->OnLoginFailure(failure);
      VLOG(1) << "Posting a QuitTask to UI thread";
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE, new MessageLoop::QuitTask);
  }

  virtual void RecoverEncryptedData(
      const std::string& old_password,
      const GaiaAuthConsumer::ClientLoginResult& credentials) {}

  virtual void ResyncEncryptedData(
      const GaiaAuthConsumer::ClientLoginResult& credentials) {}

  virtual void RetryAuth(Profile* profile,
                         const std::string& username,
                         const std::string& password,
                         const std::string& login_token,
                         const std::string& login_captcha) {}

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
                             const std::string& password,
                             const GaiaAuthConsumer::ClientLoginResult& res,
                             bool pending_requests) {
    EXPECT_EQ(expected_username_, username);
    EXPECT_EQ(expected_password_, password);
  }

  virtual void CompleteOffTheRecordLogin(const GURL& start_url) {
  }

  virtual void SetFirstLoginPrefs(PrefService* prefs) {
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

  virtual void PrewarmAuthentication() {
  }

  virtual void FetchCookies(
      Profile* profile,
      const GaiaAuthConsumer::ClientLoginResult& credentials) {
  }

  virtual void FetchTokens(
      Profile* profile,
      const GaiaAuthConsumer::ClientLoginResult& credentials) {
  }

  void SetBackgroundView(BackgroundView* background_view) {
    background_view_ = background_view;
  }

  BackgroundView* GetBackgroundView() {
    return background_view_;
  }

  virtual std::string GetOffTheRecordCommandLine(
      const GURL& start_url,
      const CommandLine& base_command_line,
      CommandLine* command_line) {
    return std::string();
  }

 private:
  std::string expected_username_;
  std::string expected_password_;
  std::string auth_token_;
  chromeos::BackgroundView* background_view_;

  DISALLOW_COPY_AND_ASSIGN(MockLoginUtils);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_
