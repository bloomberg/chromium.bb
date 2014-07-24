// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_UTILS_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chromeos/login/auth/authenticator.h"
#include "chromeos/login/auth/user_context.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

class Profile;

namespace chromeos {

class AuthStatusConsumer;

class TestLoginUtils : public LoginUtils {
 public:
  explicit TestLoginUtils(const UserContext& user_context);
  virtual ~TestLoginUtils();

  // LoginUtils:
  virtual void RespectLocalePreference(Profile* profile,
                                       const base::Closure& callback) OVERRIDE;
  virtual void DoBrowserLaunch(Profile* profile,
                               LoginDisplayHost* login_host) OVERRIDE {}
  virtual void PrepareProfile(const UserContext& user_context,
                              bool has_auth_cookies,
                              bool has_active_session,
                              Delegate* delegate) OVERRIDE;
  virtual void DelegateDeleted(Delegate* delegate) OVERRIDE;
  virtual void CompleteOffTheRecordLogin(const GURL& start_url) OVERRIDE {}
  virtual scoped_refptr<Authenticator> CreateAuthenticator(
      AuthStatusConsumer* consumer) OVERRIDE;
  virtual bool RestartToApplyPerSessionFlagsIfNeed(Profile* profile,
                                                   bool early_restart) OVERRIDE;

 private:
  UserContext expected_user_context_;

  DISALLOW_COPY_AND_ASSIGN(TestLoginUtils);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOGIN_UTILS_H_
