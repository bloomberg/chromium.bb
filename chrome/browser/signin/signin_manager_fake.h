// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FAKE_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FAKE_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/signin/signin_manager.h"

class Profile;
class ProfileKeyedService;

// A signin manager that bypasses actual authentication routines with servers
// and accepts the credentials provided to StartSignIn.
class FakeSigninManager : public SigninManager {
 public:
  explicit FakeSigninManager(Profile* profile);
  virtual ~FakeSigninManager();

  virtual void StartSignIn(const std::string& username,
                           const std::string& password,
                           const std::string& login_token,
                           const std::string& login_captcha) OVERRIDE;
  virtual void StartSignInWithCredentials(const std::string& session_index,
                                          const std::string& username,
                                          const std::string& password) OVERRIDE;
  virtual void StartSignInWithOAuth(const std::string& username,
                                    const std::string& password) OVERRIDE;
  virtual void SignOut() OVERRIDE;

  // Helper function to force a signout.
  virtual void ForceSignOut();

  virtual bool AuthInProgress() const OVERRIDE;
  void set_auth_in_progress(bool in_progress) {
    auth_in_progress_ = in_progress;
  }

  // Helper function to be used with ProfileKeyedService::SetTestingFactory().
  static ProfileKeyedService* Build(Profile* profile);

 private:
  bool auth_in_progress_;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FAKE_H_
