// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_FLOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_FLOW_H_

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/users/user.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/login/auth/auth_status_consumer.h"

namespace chromeos {

class UserContext;

class LoginDisplayHost;
// Defines possible variants of user flow upon logging in.
// See UserManager::SetUserFlow for usage contract.
class UserFlow {
 public:
  UserFlow();
  virtual ~UserFlow() = 0;
  // Indicates if screen locking should be enabled or disabled for a flow.
  virtual bool CanLockScreen() = 0;
  virtual bool ShouldShowSettings() = 0;
  virtual bool ShouldLaunchBrowser() = 0;
  virtual bool ShouldSkipPostLoginScreens() = 0;
  virtual bool HandleLoginFailure(const AuthFailure& failure) = 0;
  virtual void HandleLoginSuccess(const UserContext& context) = 0;
  virtual bool HandlePasswordChangeDetected() = 0;
  virtual void HandleOAuthTokenStatusChange(User::OAuthTokenStatus status) = 0;
  virtual void LaunchExtraSteps(Profile* profile) = 0;

  void set_host(LoginDisplayHost* host) {
    host_ = host;
  }

  LoginDisplayHost* host() {
    return host_;
  }

 private:
  LoginDisplayHost* host_;
};

// UserFlow implementation for regular login flow.
class DefaultUserFlow : public UserFlow {
 public:
  virtual ~DefaultUserFlow();

  virtual bool CanLockScreen() OVERRIDE;
  virtual bool ShouldShowSettings() OVERRIDE;
  virtual bool ShouldLaunchBrowser() OVERRIDE;
  virtual bool ShouldSkipPostLoginScreens() OVERRIDE;
  virtual bool HandleLoginFailure(const AuthFailure& failure) OVERRIDE;
  virtual void HandleLoginSuccess(const UserContext& context) OVERRIDE;
  virtual bool HandlePasswordChangeDetected() OVERRIDE;
  virtual void HandleOAuthTokenStatusChange(User::OAuthTokenStatus status)
      OVERRIDE;
  virtual void LaunchExtraSteps(Profile* profile) OVERRIDE;
};

// UserFlow stub for non-regular flows.
class ExtendedUserFlow : public UserFlow {
 public:
  explicit ExtendedUserFlow(const std::string& user_id);
  virtual ~ExtendedUserFlow();

  virtual bool ShouldShowSettings() OVERRIDE;

 protected:
  // Subclasses can call this method to unregister flow in the next event.
  virtual void UnregisterFlowSoon();
  std::string user_id() {
    return user_id_;
  }

 private:
  std::string user_id_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_FLOW_H_
