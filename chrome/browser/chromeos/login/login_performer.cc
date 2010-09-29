// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_performer.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros_settings_provider_user.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"

namespace chromeos {

namespace {
}  // namespace

LoginPerformer::LoginPerformer(Delegate* delegate)
    : last_login_failure_(LoginFailure::None()),
      delegate_(delegate) {}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, LoginStatusConsumer implementation:

void LoginPerformer::OnLoginFailure(const LoginFailure& failure) {
 last_login_failure_ = failure;
 if (delegate_) {
   captcha_.clear();
   captcha_token_.clear();
   if (failure.reason() == LoginFailure::NETWORK_AUTH_FAILED &&
       failure.error().state() == GoogleServiceAuthError::CAPTCHA_REQUIRED) {
     captcha_token_ = failure.error().captcha().token;
   }
   delegate_->OnLoginFailure(failure);
 } else {
   // TODO(nkostylev): Provide blocking UI using ScreenLocker.
 }
}

void LoginPerformer::OnLoginSuccess(const std::string& username,
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  if (delegate_) {
    delegate_->OnLoginSuccess(username, credentials);
  } else {
    // Online login has succeeded. Delete our instance.
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }
}

void LoginPerformer::OnOffTheRecordLoginSuccess() {
  if (delegate_)
    delegate_->OnOffTheRecordLoginSuccess();
  else
    NOTREACHED();
}

void LoginPerformer::OnPasswordChangeDetected(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  cached_credentials_ = credentials;
  if (delegate_) {
    delegate_->OnPasswordChangeDetected(credentials);
  } else {
    // TODO(nkostylev): Provide blocking UI using ScreenLocker.
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, SignedSettingsHelper::Callback implementation:

void LoginPerformer::OnCheckWhiteListCompleted(bool success,
                                               const std::string& email) {
  if (success) {
    // Whitelist check passed, continue with authentication.
    StartAuthentication();
  } else {
    if (delegate_)
      delegate_->WhiteListCheckFailed(email);
    else
      NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, public:

void LoginPerformer::Login(const std::string& username,
                           const std::string& password) {
  username_ = username;
  password_ = password;
  if (UserCrosSettingsProvider::cached_allow_guest()) {
    // Starts authentication if guest login is allowed.
    StartAuthentication();
  } else {
    // Otherwise, do whitelist check first.
    SignedSettingsHelper::Get()->StartCheckWhitelistOp(
        username, this);
  }
}

void LoginPerformer::LoginOffTheRecord() {
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::LoginOffTheRecord));
}

void LoginPerformer::RecoverEncryptedData(const std::string& old_password) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::RecoverEncryptedData,
                        old_password,
                        cached_credentials_));
  cached_credentials_ = GaiaAuthConsumer::ClientLoginResult();
}

void LoginPerformer::ResyncEncryptedData() {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::ResyncEncryptedData,
                        cached_credentials_));
  cached_credentials_ = GaiaAuthConsumer::ClientLoginResult();
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, private:

void LoginPerformer::StartAuthentication() {
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile();
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::AuthenticateToLogin,
                        profile,
                        username_,
                        password_,
                        captcha_token_,
                        captcha_));
  password_.clear();
}

}  // namespace chromeos
