// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth/mock_authenticator.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/login/auth/user_context.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

void MockAuthenticator::AuthenticateToLogin(Profile* profile,
                                            const UserContext& user_context) {
  if (expected_username_ == user_context.GetUserID() &&
      expected_password_ == user_context.GetPassword()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&MockAuthenticator::OnLoginSuccess, this));
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
  CHECK_EQ(expected_username_, user_context.GetUserID());
  CHECK_EQ(expected_password_, user_context.GetPassword());
  OnLoginSuccess();
}

void MockAuthenticator::AuthenticateToUnlock(
    const UserContext& user_context) {
  AuthenticateToLogin(NULL /* not used */, user_context);
}

void MockAuthenticator::LoginAsLocallyManagedUser(
    const UserContext& user_context) {
  UserContext new_user_context = user_context;
  new_user_context.SetUserIDHash(user_context.GetUserID());
  consumer_->OnLoginSuccess(new_user_context);
}

void MockAuthenticator::LoginRetailMode() {
  UserContext user_context("demo-mode");
  user_context.SetUserIDHash("demo-mode");
  consumer_->OnRetailModeLoginSuccess(user_context);
}

void MockAuthenticator::LoginAsPublicAccount(const std::string& username) {
  UserContext user_context(expected_username_);
  user_context.SetUserIDHash(expected_username_);
  consumer_->OnLoginSuccess(user_context);
}

void MockAuthenticator::LoginAsKioskAccount(const std::string& app_user_id,
                                            bool use_guest_mount) {
  UserContext user_context(expected_username_);
  user_context.SetUserIDHash(expected_username_);
  consumer_->OnLoginSuccess(user_context);
}

void MockAuthenticator::LoginOffTheRecord() {
  consumer_->OnOffTheRecordLoginSuccess();
}

void MockAuthenticator::OnRetailModeLoginSuccess() {
  UserContext user_context(expected_username_);
  user_context.SetUserIDHash(expected_username_);
  consumer_->OnRetailModeLoginSuccess(user_context);
}

void MockAuthenticator::OnLoginSuccess() {
  // If we want to be more like the real thing, we could save username
  // in AuthenticateToLogin, but there's not much of a point.
  UserContext user_context(expected_username_);
  user_context.SetPassword(expected_password_);
  user_context.SetUserIDHash(expected_username_);
  consumer_->OnLoginSuccess(user_context);
}

void MockAuthenticator::OnLoginFailure(const LoginFailure& failure) {
    consumer_->OnLoginFailure(failure);
}

void MockAuthenticator::SetExpectedCredentials(
    const std::string& expected_username,
    const std::string& expected_password) {
  expected_username_ = expected_username;
  expected_password_ = expected_password;
}

}  // namespace chromeos
