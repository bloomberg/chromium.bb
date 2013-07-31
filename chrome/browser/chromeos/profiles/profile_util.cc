// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/profiles/profile_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chromeos/login/login_state.h"

namespace chromeos {

bool IsProfileAssociatedWithGaiaAccount(Profile* profile) {
  if (!chromeos::LoginState::IsInitialized())
    return false;
  if (!chromeos::LoginState::Get()->IsUserGaiaAuthenticated())
    return false;
  if (profile->IsOffTheRecord())
    return false;
  return true;
}

}  // namespace chromeos
