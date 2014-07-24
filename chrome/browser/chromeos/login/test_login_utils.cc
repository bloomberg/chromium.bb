// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test_login_utils.h"

#include "base/callback.h"
#include "base/logging.h"
#include "chromeos/login/auth/mock_authenticator.h"
#include "chromeos/login/auth/user_context.h"

namespace chromeos {

TestLoginUtils::TestLoginUtils(const UserContext& user_context)
    : expected_user_context_(user_context) {
}

TestLoginUtils::~TestLoginUtils() {}

void TestLoginUtils::RespectLocalePreference(Profile* profile,
                                             const base::Closure& callback) {
  callback.Run();
}

void TestLoginUtils::PrepareProfile(
    const UserContext& user_context,
    bool has_auth_cookies,
    bool has_active_session,
    Delegate* delegate) {
  if (user_context != expected_user_context_)
    NOTREACHED();
  // Profile hasn't been loaded.
  delegate->OnProfilePrepared(NULL);
}

void TestLoginUtils::DelegateDeleted(Delegate* delegate) {
}

scoped_refptr<Authenticator> TestLoginUtils::CreateAuthenticator(
    AuthStatusConsumer* consumer) {
  return new MockAuthenticator(consumer, expected_user_context_);
}

bool TestLoginUtils::RestartToApplyPerSessionFlagsIfNeed(Profile* profile,
                                                         bool early_restart) {
  return false;
}

}  // namespace chromeos
