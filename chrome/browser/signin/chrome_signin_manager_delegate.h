// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_MANAGER_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/signin/signin_manager_delegate.h"

class CookieSettings;
class Profile;

class ChromeSigninManagerDelegate : public SigninManagerDelegate {
 public:
  explicit ChromeSigninManagerDelegate(Profile* profile);
  virtual ~ChromeSigninManagerDelegate();

  // Utility methods.
  static bool ProfileAllowsSigninCookies(Profile* profile);
  static bool SettingsAllowSigninCookies(CookieSettings* cookie_settings);

  // SigninManagerDelegate implementation.
  virtual bool AreSigninCookiesAllowed() OVERRIDE;

 private:
  // Non-owning; this object is owned by the SigninManager, which is
  // outlived by Profile.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSigninManagerDelegate);
};

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_MANAGER_DELEGATE_H_
