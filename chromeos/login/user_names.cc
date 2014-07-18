// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/login/user_names.h"

#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

namespace login {

const char* kStubUser = "stub-user@example.com";

const char* kSignInUser = "sign-in-user-id";

// Should match cros constant in platform/libchromeos/chromeos/cryptohome.h
const char* kGuestUserName = "$guest";

const char* kSupervisedUserDomain = "locally-managed.localhost";

const char* kRetailModeUserName = "demouser@";

std::string CanonicalizeUserID(const std::string& user_id) {
  if (user_id == chromeos::login::kGuestUserName)
    return user_id;
  return gaia::CanonicalizeEmail(user_id);
}

}  // namespace login

}  // namespace chromeos
