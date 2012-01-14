// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FAKE_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FAKE_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/signin/signin_manager.h"

// A signin manager that bypasses actual authentication routines with servers
// and accepts the credentials provided to StartSignIn.
class FakeSigninManager : public SigninManager {
 public:
  FakeSigninManager();
  virtual ~FakeSigninManager();

  virtual void StartSignIn(const std::string& username,
                           const std::string& password,
                           const std::string& login_token,
                           const std::string& login_captcha) OVERRIDE;
  virtual void SignOut() OVERRIDE;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_MANAGER_FAKE_H_
