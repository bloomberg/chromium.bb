// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CLIENT_H_
#define COMPONENTS_PROXIMITY_AUTH_CLIENT_H_

#include <string>

#include "components/proximity_auth/screenlock_state.h"

namespace proximity_auth {

// An interface that needs to be supplied to the Proximity Auth component by its
// embedder. There should be one |ProximityAuthClient| per
// |content::BrowserContext|.
class ProximityAuthClient {
 public:
  virtual ~ProximityAuthClient() {}

  // Returns the authenticated username.
  virtual std::string GetAuthenticatedUsername() const = 0;

  // Updates the user pod on the signin or lock screen to reflect the provided
  // screenlock state.
  virtual void UpdateScreenlockState(ScreenlockState state) = 0;

  // Finalizes an unlock attempt initiated by the user. If |success| is true,
  // the screen is unlocked; otherwise, the auth attempt is rejected. An auth
  // attempt must be in progress before calling this function.
  virtual void FinalizeUnlock(bool success) = 0;

  // Finalizes a sign-in attempt initiated by the user. If |success| is true,
  // the user is signed in; otherwise, the auth attempt is rejected. An auth
  // attempt must be in progress before calling this function.
  virtual void FinalizeSignin(const std::string& secret) = 0;
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CLIENT_H_
