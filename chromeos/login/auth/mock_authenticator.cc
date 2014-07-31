// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/mock_authenticator.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"

namespace chromeos {

MockAuthenticator::MockAuthenticator(AuthStatusConsumer* consumer,
                                     const UserContext& expected_user_context)
    : Authenticator(consumer),
      expected_user_context_(expected_user_context),
      message_loop_(base::MessageLoopProxy::current()) {
}

void MockAuthenticator::CompleteLogin(Profile* profile,
                                      const UserContext& user_context) {
  if (expected_user_context_ != user_context)
    NOTREACHED();
  OnAuthSuccess();
}

void MockAuthenticator::AuthenticateToLogin(Profile* profile,
                                            const UserContext& user_context) {
  if (user_context == expected_user_context_) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&MockAuthenticator::OnAuthSuccess, this));
    return;
  }
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&MockAuthenticator::OnAuthFailure,
                 this,
                 AuthFailure::FromNetworkAuthFailure(error)));
}

void MockAuthenticator::AuthenticateToUnlock(const UserContext& user_context) {
  AuthenticateToLogin(NULL /* not used */, user_context);
}

void MockAuthenticator::LoginAsSupervisedUser(const UserContext& user_context) {
  UserContext new_user_context = user_context;
  new_user_context.SetUserIDHash(user_context.GetUserID());
  consumer_->OnAuthSuccess(new_user_context);
}

void MockAuthenticator::LoginRetailMode() {
  UserContext user_context("demo-mode");
  user_context.SetUserIDHash("demo-mode");
  consumer_->OnRetailModeAuthSuccess(user_context);
}

void MockAuthenticator::LoginOffTheRecord() {
  consumer_->OnOffTheRecordAuthSuccess();
}

void MockAuthenticator::LoginAsPublicSession(const UserContext& user_context) {
  UserContext logged_in_user_context = user_context;
  logged_in_user_context.SetUserIDHash(logged_in_user_context.GetUserID());
  consumer_->OnAuthSuccess(logged_in_user_context);
}

void MockAuthenticator::LoginAsKioskAccount(const std::string& app_user_id,
                                            bool use_guest_mount) {
  UserContext user_context(expected_user_context_.GetUserID());
  user_context.SetUserIDHash(expected_user_context_.GetUserID());
  consumer_->OnAuthSuccess(user_context);
}

void MockAuthenticator::OnRetailModeAuthSuccess() {
  UserContext user_context(expected_user_context_.GetUserID());
  user_context.SetUserIDHash(expected_user_context_.GetUserID());
  consumer_->OnRetailModeAuthSuccess(user_context);
}

void MockAuthenticator::OnAuthSuccess() {
  // If we want to be more like the real thing, we could save the user ID
  // in AuthenticateToLogin, but there's not much of a point.
  UserContext user_context(expected_user_context_);
  user_context.SetUserIDHash(expected_user_context_.GetUserID());
  consumer_->OnAuthSuccess(user_context);
}

void MockAuthenticator::OnAuthFailure(const AuthFailure& failure) {
  consumer_->OnAuthFailure(failure);
}

void MockAuthenticator::RecoverEncryptedData(const std::string& old_password) {
}

void MockAuthenticator::ResyncEncryptedData() {
}

void MockAuthenticator::SetExpectedCredentials(
    const UserContext& user_context) {
  expected_user_context_ = user_context;
}

MockAuthenticator::~MockAuthenticator() {
}

}  // namespace chromeos
