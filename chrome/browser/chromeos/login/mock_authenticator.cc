// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_authenticator.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

void MockAuthenticator::AuthenticateToLogin(Profile* profile,
                                 const std::string& username,
                                 const std::string& password,
                                 const std::string& login_token,
                                 const std::string& login_captcha) {
  if (expected_username_ == username && expected_password_ == password) {
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
                                      const std::string& username,
                                      const std::string& password) {
  CHECK_EQ(expected_username_, username);
  CHECK_EQ(expected_password_, password);
  OnLoginSuccess(false);
}

void MockAuthenticator::AuthenticateToUnlock(const std::string& username,
                                  const std::string& password) {
  AuthenticateToLogin(NULL /* not used */, username, password,
                      std::string(), std::string());
}

void MockAuthenticator::LoginRetailMode() {
  consumer_->OnRetailModeLoginSuccess();
}

void MockAuthenticator::LoginAsPublicAccount(const std::string& username) {
  consumer_->OnLoginSuccess(expected_username_, "", false, false);
}

void MockAuthenticator::LoginOffTheRecord() {
  consumer_->OnOffTheRecordLoginSuccess();
}

void MockAuthenticator::OnRetailModeLoginSuccess() {
  consumer_->OnRetailModeLoginSuccess();
}

void MockAuthenticator::OnLoginSuccess(
    bool request_pending) {
  // If we want to be more like the real thing, we could save username
  // in AuthenticateToLogin, but there's not much of a point.
  consumer_->OnLoginSuccess(expected_username_,
                            expected_password_,
                            request_pending,
                            false);
}

void MockAuthenticator::OnLoginFailure(const LoginFailure& failure) {
    consumer_->OnLoginFailure(failure);
}

////////////////////////////////////////////////////////////////////////////////
// TestLoginUtils

TestLoginUtils::TestLoginUtils(const std::string& expected_username,
                               const std::string& expected_password)
    : expected_username_(expected_username),
      expected_password_(expected_password) {
}

TestLoginUtils::~TestLoginUtils() {}

void TestLoginUtils::PrepareProfile(
    const std::string& username,
    const std::string& display_email,
    const std::string& password,
    bool pending_requests,
    bool using_oauth,
    bool has_cookies,
    Delegate* delegate) {
  DCHECK_EQ(expected_username_, username);
  DCHECK_EQ(expected_password_, password);
  // Profile hasn't been loaded.
  delegate->OnProfilePrepared(NULL);
}

void TestLoginUtils::DelegateDeleted(Delegate* delegate) {
}

scoped_refptr<Authenticator> TestLoginUtils::CreateAuthenticator(
    LoginStatusConsumer* consumer) {
  return new MockAuthenticator(
      consumer, expected_username_, expected_password_);
}

std::string TestLoginUtils::GetOffTheRecordCommandLine(
    const GURL& start_url,
    const CommandLine& base_command_line,
    CommandLine* command_line) {
  return std::string();
}

void TestLoginUtils::TransferDefaultCookiesAndServerBoundCerts(
    Profile* default_profile,
    Profile* new_profile) {
}

void TestLoginUtils::TransferDefaultAuthCache(Profile* default_profile,
                                              Profile* new_profile) {
}

void TestLoginUtils::StopBackgroundFetchers() {
}

}  // namespace chromeos
