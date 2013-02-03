// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FAKE_AUTH_STATUS_PROVIDER_H_
#define CHROME_BROWSER_SIGNIN_FAKE_AUTH_STATUS_PROVIDER_H_

#include "chrome/browser/signin/signin_global_error.h"

// Helper class that reports auth errors to SigninGlobalError. Automatically
// registers and de-registers itself as an AuthStatusProvider in the
// constructor and destructor.
class FakeAuthStatusProvider : public SigninGlobalError::AuthStatusProvider {
 public:
  explicit FakeAuthStatusProvider(SigninGlobalError* error);
  virtual ~FakeAuthStatusProvider();

  // Sets the auth error that this provider reports to SigninGlobalError. Also
  // notifies SigninGlobalError via AuthStatusChanged().
  void SetAuthError(const GoogleServiceAuthError& error);

  // AuthStatusProvider implementation.
  virtual GoogleServiceAuthError GetAuthStatus() const OVERRIDE;

 private:
  SigninGlobalError* global_error_;
  GoogleServiceAuthError auth_error_;
};

#endif  // CHROME_BROWSER_SIGNIN_FAKE_AUTH_STATUS_PROVIDER_H_
