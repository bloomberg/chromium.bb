// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_FLOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_FLOW_H_

#include "base/compiler_specific.h"

namespace chromeos {

// Defines possible variants of user flow upon logging in.
// See UserManager::SetUserFlow for usage contract.
class UserFlow {
 public:
  virtual ~UserFlow() = 0;
  virtual bool ShouldLaunchBrowser() = 0;
  virtual bool ShouldSkipPostLoginScreens() = 0;
  virtual void LaunchExtraSteps() = 0;
};

// UserFlow implementation for regular login flow.
class DefaultUserFlow : public UserFlow {
 public:
  virtual ~DefaultUserFlow();

  virtual bool ShouldLaunchBrowser() OVERRIDE;
  virtual bool ShouldSkipPostLoginScreens() OVERRIDE;
  virtual void LaunchExtraSteps() OVERRIDE;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_FLOW_H_
