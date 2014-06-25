// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_PROFILE_AUTH_DATA_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_PROFILE_AUTH_DATA_H_

#include "base/callback_forward.h"
#include "base/macros.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

// Helper class that transfers authentication-related data from a BrowserContext
// used for authentication to the user's actual BrowserContext.
class ProfileAuthData {
 public:
  // Transfers authentication-related data from |from_context| to |to_context|
  // and invokes |completion_callback| on the UI thread when the operation has
  // completed. The proxy authentication state is transferred unconditionally.
  // If |transfer_auth_cookies_and_server_bound_certs| is true, authentication
  // cookies and server bound certificates are transferred as well, if
  // |to_context|'s cookie jar is empty. If the cookie jar is not empty, the
  // authentication states in |from_context| and |to_context| should be merged
  // using /MergeSession instead.
  static void Transfer(content::BrowserContext* from_context,
                       content::BrowserContext* to_context,
                       bool transfer_auth_cookies_and_server_bound_certs,
                       const base::Closure& completion_callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ProfileAuthData);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_PROFILE_AUTH_DATA_H_
