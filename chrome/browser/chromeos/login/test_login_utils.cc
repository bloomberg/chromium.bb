// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test_login_utils.h"

#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/login/user.h"

namespace chromeos {

TestLoginUtils::TestLoginUtils(const std::string& expected_username,
                               const std::string& expected_password)
    : expected_username_(expected_username),
      expected_password_(expected_password) {
}

TestLoginUtils::~TestLoginUtils() {}

void TestLoginUtils::PrepareProfile(
    const UserContext& credentials,
    const std::string& display_email,
    bool using_oauth,
    bool has_cookies,
    Delegate* delegate) {
  DCHECK_EQ(expected_username_, credentials.username);
  DCHECK_EQ(expected_password_, credentials.password);
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

void TestLoginUtils::InitRlzDelayed(Profile* user_profile) {
}

void TestLoginUtils::StopBackgroundFetchers() {
}

}  // namespace chromeos
