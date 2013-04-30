// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FAKE_SIGNIN_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_FAKE_SIGNIN_MANAGER_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/signin/signin_manager.h"

class Profile;
class ProfileKeyedService;

class FakeSigninManagerBase : public SigninManagerBase {
 public:
  explicit FakeSigninManagerBase(Profile* profile);
  virtual ~FakeSigninManagerBase();

  virtual void SignOut() OVERRIDE;

  // Helper function to be used with ProfileKeyedService::SetTestingFactory().
  static ProfileKeyedService* Build(content::BrowserContext* profile);
};

#if !defined(OS_CHROMEOS)

// A signin manager that bypasses actual authentication routines with servers
// and accepts the credentials provided to StartSignIn.
class FakeSigninManager : public SigninManager {
 public:
  explicit FakeSigninManager(Profile* profile);
  virtual ~FakeSigninManager();

  void set_auth_in_progress(const std::string& username) {
    possibly_invalid_username_ = username;
  }

  virtual void SignOut() OVERRIDE;
  virtual void InitTokenService() OVERRIDE;
  virtual void StartSignIn(const std::string& username,
                           const std::string& password,
                           const std::string& login_token,
                           const std::string& login_captcha) OVERRIDE;

  virtual void StartSignInWithCredentials(
      const std::string& session_index,
      const std::string& username,
      const std::string& password,
      const OAuthTokenFetchedCallback& oauth_fetched_callback) OVERRIDE;

  virtual void CompletePendingSignin() OVERRIDE;

  // Helper function to be used with ProfileKeyedService::SetTestingFactory().
  static ProfileKeyedService* Build(content::BrowserContext* profile);
};

#endif  // !defined (OS_CHROMEOS)

#endif  // CHROME_BROWSER_SIGNIN_FAKE_SIGNIN_MANAGER_H_
