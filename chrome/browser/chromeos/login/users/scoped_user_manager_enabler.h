// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_SCOPED_USER_MANAGER_ENABLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_SCOPED_USER_MANAGER_ENABLER_H_

#include "base/macros.h"

namespace user_manager {
class UserManager;
}

namespace chromeos {

class ChromeUserManager;

// Helper class for unit tests. Initializes the UserManager singleton to the
// given |user_manager| and tears it down again on destruction. If the singleton
// had already been initialized, its previous value is restored after tearing
// down |user_manager|.
class ScopedUserManagerEnabler {
 public:
  // Takes ownership of |user_manager|.
  explicit ScopedUserManagerEnabler(ChromeUserManager* user_manager);
  ~ScopedUserManagerEnabler();

 private:
  user_manager::UserManager* previous_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUserManagerEnabler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_SCOPED_USER_MANAGER_ENABLER_H_
