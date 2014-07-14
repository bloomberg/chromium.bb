// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_FAKE_LOGIN_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_FAKE_LOGIN_UTILS_H_

#include "chrome/browser/chromeos/login/login_utils.h"
#include "chromeos/login/auth/user_context.h"

namespace chromeos {

// This class emulates behavior of LoginUtils for browser tests.
// It provides:
//  * Fake authentication. You can configure expected usernames and password for
//    next auth attempt.
//  * Preparing of profiles for authenticated users.
//  * Launching browser for user, if |should_launch_browser_| set.
//  * Correct communication with LoginDisplayHost and UserManager.
class FakeLoginUtils : public LoginUtils {
 public:
  FakeLoginUtils();
  virtual ~FakeLoginUtils();
  virtual void DoBrowserLaunch(Profile* profile,
                               LoginDisplayHost* login_host) OVERRIDE;
  virtual void PrepareProfile(const UserContext& user_context,
                              bool has_cookies,
                              bool has_active_session,
                              LoginUtils::Delegate* delegate) OVERRIDE;
  virtual void DelegateDeleted(LoginUtils::Delegate* delegate) OVERRIDE;
  virtual void CompleteOffTheRecordLogin(const GURL& start_url) OVERRIDE;
  virtual scoped_refptr<Authenticator> CreateAuthenticator(
      AuthStatusConsumer* consumer) OVERRIDE;
  virtual bool RestartToApplyPerSessionFlagsIfNeed(Profile* profile,
                                                   bool early_restart) OVERRIDE;

  void SetExpectedCredentials(const UserContext& user_context);
  void set_should_launch_browser(bool should_launch_browser) {
    should_launch_browser_ = should_launch_browser;
  }

 private:
  scoped_refptr<Authenticator> authenticator_;
  UserContext expected_user_context_;
  bool should_launch_browser_;

  DISALLOW_COPY_AND_ASSIGN(FakeLoginUtils);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_FAKE_LOGIN_UTILS_H_
