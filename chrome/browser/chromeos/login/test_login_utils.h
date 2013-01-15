// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_UTILS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

class Profile;

namespace chromeos {

class LoginStatusConsumer;

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

  virtual std::string GetOffTheRecordCommandLine(
      const GURL& start_url,
      const CommandLine& base_command_line,
      CommandLine* command_line) OVERRIDE;

  virtual void InitRlzDelayed(Profile* user_profile) OVERRIDE;

  virtual void StopBackgroundFetchers() OVERRIDE;

 private:
  std::string expected_username_;
  std::string expected_password_;
  std::string auth_token_;

  DISALLOW_COPY_AND_ASSIGN(TestLoginUtils);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_UTILS_H_
