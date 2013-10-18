// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_EVENT_ROUTER_H_

#include <string>

#include "base/basictypes.h"

class Profile;

namespace extensions {

class IdentityEventRouter {
 public:
  explicit IdentityEventRouter(Profile* profile);
  ~IdentityEventRouter();

  // Dispatch identity.onSignInChanged event, including email address
  // for extensions with the identity.email permission.
  void DispatchSignInEvent(const std::string& id,
                           const std::string& email,
                           bool is_signed_in);

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(IdentityEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_EVENT_ROUTER_H_
