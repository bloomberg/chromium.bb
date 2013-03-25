// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_FLOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_FLOW_H_

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {

class LoginDisplayHost;
// Defines possible variants of user flow upon logging in.
// See UserManager::SetUserFlow for usage contract.
class UserFlow {
 public:
  virtual ~UserFlow() = 0;
  virtual bool ShouldLaunchBrowser() = 0;
  virtual bool ShouldSkipPostLoginScreens() = 0;
  virtual bool HandleLoginFailure(const LoginFailure& failure,
      LoginDisplayHost* host) = 0;
  virtual bool HandlePasswordChangeDetected(LoginDisplayHost* host) = 0;
  virtual void LaunchExtraSteps(Profile* profile, LoginDisplayHost* host) = 0;
};

// UserFlow implementation for regular login flow.
class DefaultUserFlow : public UserFlow {
 public:
  virtual ~DefaultUserFlow();

  virtual bool ShouldLaunchBrowser() OVERRIDE;
  virtual bool ShouldSkipPostLoginScreens() OVERRIDE;
  virtual bool HandleLoginFailure(const LoginFailure& failure,
      LoginDisplayHost* host) OVERRIDE;
  virtual bool HandlePasswordChangeDetected(LoginDisplayHost* host) OVERRIDE;
  virtual void LaunchExtraSteps(Profile* profile,
      LoginDisplayHost* host) OVERRIDE;
};

// UserFlow stub for non-regular flows.
class ExtendedUserFlow : public UserFlow {
 public:
  explicit ExtendedUserFlow(const std::string& user_id);
  virtual ~ExtendedUserFlow();

 protected:
  // Subclasses can call this method to unregister flow in the next event.
  virtual void UnregisterFlowSoon();
  std::string user_id() {
    return user_id_;
  };

 private:
  std::string user_id_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_FLOW_H_
