// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_PROFILE_AUTH_DATA_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_PROFILE_AUTH_DATA_H_

#include <string>
#include "base/callback.h"

class Profile;

namespace chromeos {

// Helper class for transferring authentication related data from one profile
// to another: proxy authentication cache, cookies, server bound certs.
class ProfileAuthData {
 public:
  // Transfers proxy authentication cache and optionally |transfer_cookies| and
  // server bound certs from the profile that was used for authentication.
  // |cookies_transfered_callback| will be called on UI thread after cookie
  // transfer part of this operation is completed.
  static void Transfer(Profile* from_profile,
                       Profile* to_profile,
                       bool transfer_cookies,
                       const base::Closure& cookies_transfered_callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ProfileAuthData);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_PROFILE_AUTH_DATA_H_
