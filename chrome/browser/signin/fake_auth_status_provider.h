// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_FAKE_AUTH_STATUS_PROVIDER_H_
#define CHROME_BROWSER_SIGNIN_FAKE_AUTH_STATUS_PROVIDER_H_

#include "components/signin/core/signin_error_controller.h"

// Helper class that reports auth errors to SigninErrorController. Automatically
// registers and de-registers itself as an AuthStatusProvider in the
// constructor and destructor.
class FakeAuthStatusProvider
    : public SigninErrorController::AuthStatusProvider {
 public:
  explicit FakeAuthStatusProvider(SigninErrorController* error);
  virtual ~FakeAuthStatusProvider();

  // Sets the auth error that this provider reports to SigninErrorController.
  // Also notifies SigninErrorController via AuthStatusChanged().
  void SetAuthError(const std::string& account_id,
                    const GoogleServiceAuthError& error);

  void set_error_without_status_change(const GoogleServiceAuthError& error) {
    auth_error_ = error;
  }

  // AuthStatusProvider implementation.
  virtual std::string GetAccountId() const OVERRIDE;
  virtual GoogleServiceAuthError GetAuthStatus() const OVERRIDE;

 private:
  SigninErrorController* error_provider_;
  std::string account_id_;
  GoogleServiceAuthError auth_error_;
};

#endif  // CHROME_BROWSER_SIGNIN_FAKE_AUTH_STATUS_PROVIDER_H_
