// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROFILES_PROFILE_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_PROFILES_PROFILE_HELPER_H_

#include "base/basictypes.h"

class Profile;

namespace chromeos {

class ProfileHelper {
 public:
  // Returns OffTheRecord profile for use during signing phase.
  static Profile* GetSigninProfile();

  // Initialize a bunch of services that are tied to a browser profile.
  // TODO(dzhioev): Investigate whether or not this method is needed.
  static void ProfileStartup(Profile* profile, bool process_startup);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ProfileHelper);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROFILES_PROFILE_HELPER_H_

