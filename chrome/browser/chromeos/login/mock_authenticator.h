// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/google_service_auth_error.h"
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
                             const std::string& password) OVERRIDE;

  virtual void AuthenticateToLogin(Profile* profile,
                                   const std::string& username,
                                   const std::string& password,
                                   const std::string& login_token,
                                   const std::string& login_captcha) OVERRIDE;

  virtual void AuthenticateToUnlock(const std::string& username,
                                    const std::string& password) OVERRIDE;

  virtual void LoginDemoUser() OVERRIDE;
  virtual void LoginOffTheRecord() OVERRIDE;

  virtual void OnDemoUserLoginSuccess() OVERRIDE;

  virtual void OnLoginSuccess(bool request_pending) OVERRIDE;

  virtual void OnLoginFailure(const LoginFailure& failure) OVERRIDE;

  virtual void RecoverEncryptedData(
      const std::string& old_password) OVERRIDE {}

  virtual void ResyncEncryptedData() OVERRIDE {}

  virtual void RetryAuth(Profile* profile,
                         const std::string& username,
                         const std::string& password,
                         const std::string& login_token,
                         const std::string& login_captcha) OVERRIDE {}

 protected:
  virtual ~MockAuthenticator() {}

 private:
  std::string expected_username_;
  std::string expected_password_;

  DISALLOW_COPY_AND_ASSIGN(MockAuthenticator);
};

class TestLoginUtils : public LoginUtils {
 public:
  TestLoginUtils(const std::string& expected_username,
                 const std::string& expected_password);
  virtual ~TestLoginUtils();

  virtual void DoBrowserLaunch(Profile* profile,
                                 LoginDisplayHost* login_host) OVERRIDE {}
  virtual void PrepareProfile(const std::string& username,
                              const std::string& display_email,
                              const std::string& password,
                              bool pending_requests,
                              bool using_oauth,
                              bool has_cookies,
                              Delegate* delegate) OVERRIDE;

  virtual void DelegateDeleted(Delegate* delegate) OVERRIDE;

  virtual void CompleteOffTheRecordLogin(const GURL& start_url) OVERRIDE {}

  virtual void SetFirstLoginPrefs(PrefService* prefs) OVERRIDE {}

  virtual scoped_refptr<Authenticator> CreateAuthenticator(
      LoginStatusConsumer* consumer) OVERRIDE;

  virtual void PrewarmAuthentication() OVERRIDE {}

  virtual void RestoreAuthenticationSession(Profile* profile) OVERRIDE {}

  virtual void StartTokenServices(Profile* profile) OVERRIDE {}

  virtual void StartSignedInServices(
      Profile* profile,
      const GaiaAuthConsumer::ClientLoginResult& credentials) OVERRIDE {}

  virtual std::string GetOffTheRecordCommandLine(
      const GURL& start_url,
      const CommandLine& base_command_line,
      CommandLine* command_line) OVERRIDE;

  virtual void TransferDefaultCookies(Profile* default_profile,
                                      Profile* new_profile) OVERRIDE;

  virtual void TransferDefaultAuthCache(Profile* default_profile,
                                        Profile* new_profile) OVERRIDE;

  virtual void StopBackgroundFetchers() OVERRIDE;

 private:
  std::string expected_username_;
  std::string expected_password_;
  std::string auth_token_;

  DISALLOW_COPY_AND_ASSIGN(TestLoginUtils);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_AUTHENTICATOR_H_
