// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_authenticator.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/login/user.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

void MockAuthenticator::AuthenticateToLogin(Profile* profile,
                                            const UserContext& user_context,
                                            const std::string& login_token,
                                            const std::string& login_captcha) {
  if (expected_username_ == user_context.username &&
      expected_password_ == user_context.password) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&MockAuthenticator::OnLoginSuccess, this, false));
    return;
  }
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&MockAuthenticator::OnLoginFailure, this,
                 LoginFailure::FromNetworkAuthFailure(error)));
}

void MockAuthenticator::CompleteLogin(Profile* profile,
                                      const UserContext& user_context) {
  CHECK_EQ(expected_username_, user_context.username);
  CHECK_EQ(expected_password_, user_context.password);
  OnLoginSuccess(false);
}

void MockAuthenticator::AuthenticateToUnlock(
    const UserContext& user_context) {
  AuthenticateToLogin(NULL /* not used */, user_context,
                      std::string(), std::string());
}

void MockAuthenticator::LoginAsLocallyManagedUser(
    const UserContext& user_context) {
  consumer_->OnLoginSuccess(UserContext(expected_username_,
                                        std::string(),
                                        std::string(),
                                        user_context.username), // username_hash
                            false,
                            false);
}

void MockAuthenticator::LoginRetailMode() {
  consumer_->OnRetailModeLoginSuccess(UserContext("demo-mode",
                                                  std::string(),
                                                  std::string(),
                                                  "demo-mode"));
}

void MockAuthenticator::LoginAsPublicAccount(const std::string& username) {
  consumer_->OnLoginSuccess(UserContext(expected_username_,
                                        std::string(),
                                        std::string(),
                                        expected_username_),
                            false,
                            false);
}

void MockAuthenticator::LoginOffTheRecord() {
  consumer_->OnOffTheRecordLoginSuccess();
}

void MockAuthenticator::OnRetailModeLoginSuccess() {
  consumer_->OnRetailModeLoginSuccess(UserContext(expected_username_,
                                                  std::string(),
                                                  std::string(),
                                                  expected_username_));
}

void MockAuthenticator::OnLoginSuccess(bool request_pending) {
  // If we want to be more like the real thing, we could save username
  // in AuthenticateToLogin, but there's not much of a point.
  consumer_->OnLoginSuccess(UserContext(expected_username_,
                                        expected_password_,
                                        std::string(),
                                        expected_username_),
                            request_pending,
                            false);
}

void MockAuthenticator::OnLoginFailure(const LoginFailure& failure) {
    consumer_->OnLoginFailure(failure);
}

}  // namespace chromeos
