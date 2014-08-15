// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_SCOPED_TEST_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_SCOPED_TEST_USER_MANAGER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace chromeos {

class ChromeUserManager;

// Helper class for unit tests. Initializes the UserManager singleton on
// construction and tears it down again on destruction.
class ScopedTestUserManager {
 public:
  ScopedTestUserManager();
  ~ScopedTestUserManager();

 private:
  scoped_ptr<ChromeUserManager> chrome_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTestUserManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_SCOPED_TEST_USER_MANAGER_H_
