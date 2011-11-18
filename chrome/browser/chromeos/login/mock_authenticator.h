// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/public/browser/browser_thread.h"
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

  virtual void CompleteLogin(Profile* profile,
                             const std::string& username,
                             const std::string& password);

  virtual void AuthenticateToLogin(Profile* profile,
                                   const std::string& username,
                                   const std::string& password,
                                   const std::string& login_token,
                                   const std::string& login_captcha);

  virtual void AuthenticateToUnlock(const std::string& username,
                                    const std::string& password);

  virtual void LoginOffTheRecord();

  virtual void OnLoginSuccess(
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      bool request_pending);

  virtual void OnLoginFailure(const LoginFailure& failure);

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

  virtual std::string EncryptToken(const std::string& token);

  virtual std::string DecryptToken(const std::string& encrypted_token);

  virtual void VerifyOAuth1AccessToken(const std::string& oauth1_access_token,
                                       const std::string& oauth1_secret) {}

 private:
  std::string expected_username_;
  std::string expected_password_;

  DISALLOW_COPY_AND_ASSIGN(MockAuthenticator);
};

class MockLoginUtils : public LoginUtils {
 public:
  MockLoginUtils(const std::string& expected_username,
                 const std::string& expected_password);
  virtual ~MockLoginUtils();

  virtual bool ShouldWaitForWifi();

  virtual void PrepareProfile(const std::string& username,
                              const std::string& password,
                              const GaiaAuthConsumer::ClientLoginResult& res,
                              bool pending_requests,
                              bool using_oauth,
                              bool has_cookies,
                              Delegate* delegate);

  virtual void DelegateDeleted(Delegate* delegate);

  virtual void CompleteOffTheRecordLogin(const GURL& start_url) {}

  virtual void SetFirstLoginPrefs(PrefService* prefs) {}

  virtual scoped_refptr<Authenticator> CreateAuthenticator(
      LoginStatusConsumer* consumer);

  virtual void PrewarmAuthentication() {}

  virtual void RestoreAuthenticationSession(const std::string& user_name,
                                            Profile* profile) {}

  virtual void StartTokenServices(Profile* profile) {}

  virtual void StartSync(
      Profile* profile,
      const GaiaAuthConsumer::ClientLoginResult& credentials) {}

  virtual void SetBackgroundView(BackgroundView* background_view);

  virtual BackgroundView* GetBackgroundView();

  virtual std::string GetOffTheRecordCommandLine(
      const GURL& start_url,
      const CommandLine& base_command_line,
      CommandLine* command_line);

  virtual void TransferDefaultCookies(Profile* default_profile,
                                      Profile* new_profile) OVERRIDE;

  virtual void TransferDefaultAuthCache(Profile* default_profile,
                                        Profile* new_profile) OVERRIDE;

 private:
  std::string expected_username_;
  std::string expected_password_;
  std::string auth_token_;
  chromeos::BackgroundView* background_view_;

  DISALLOW_COPY_AND_ASSIGN(MockLoginUtils);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_
