// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CLIENT_H_
#define COMPONENTS_PROXIMITY_AUTH_CLIENT_H_

#include <string>

namespace proximity_auth {

// An interface that needs to be supplied to the Proximity Auth component by its
// embedder. There should be one |ProximityAuthClient| per
// |content::BrowserContext|.
class ProximityAuthClient {
 public:
  virtual ~ProximityAuthClient() {}

  // Returns the authenticated username.
  virtual std::string GetAuthenticatedUsername() const = 0;

  // Finalizes an unlock attempt initiated by the user. If |success| is true,
  // the screen is unlocked; otherwise, the auth attempt is rejected. An auth
  // attempt must be in progress before calling this function.
  virtual void FinalizeUnlock(bool success) = 0;
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CLIENT_H_
