// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/scoped_test_public_session_login_state.h"

#include "chromeos/login/login_state.h"

namespace chromeos {

ScopedTestPublicSessionLoginState::ScopedTestPublicSessionLoginState() {
  // Set Public Session state.
  chromeos::LoginState::Initialize();
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
}

ScopedTestPublicSessionLoginState::~ScopedTestPublicSessionLoginState() {
  // Reset state at the end of test.
  chromeos::LoginState::Shutdown();
}

}  // namespace chromeos
