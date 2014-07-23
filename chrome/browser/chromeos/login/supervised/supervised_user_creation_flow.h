// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_CREATION_FLOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_CREATION_FLOW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "components/user_manager/user.h"

class Profile;

namespace chromeos {

// UserFlow implementation for creating new supervised user.
class SupervisedUserCreationFlow : public ExtendedUserFlow {
 public:
  explicit SupervisedUserCreationFlow(const std::string& manager_id);
  virtual ~SupervisedUserCreationFlow();

  virtual bool CanLockScreen() OVERRIDE;
  virtual bool ShouldShowSettings() OVERRIDE;
  virtual bool ShouldLaunchBrowser() OVERRIDE;
  virtual bool ShouldSkipPostLoginScreens() OVERRIDE;
  virtual bool HandleLoginFailure(const AuthFailure& failure) OVERRIDE;
  virtual void HandleLoginSuccess(const UserContext& context) OVERRIDE;
  virtual bool HandlePasswordChangeDetected() OVERRIDE;
  virtual void HandleOAuthTokenStatusChange(
      user_manager::User::OAuthTokenStatus status) OVERRIDE;
  virtual void LaunchExtraSteps(Profile* profile) OVERRIDE;
 private:
  // Display name for user being created.
  base::string16 name_;
  // Password for user being created.
  std::string password_;

  // Indicates if manager OAuth2 token has been validated.
  bool token_validated_;

  // Indicates if manager was successfully authenticated against
  // local cryptohome.
  bool logged_in_;

  // Indicates that cryptohome is mounted and OAuth2 token is validated.
  // Used to avoid multiple notifications.
  bool session_started_;

  Profile* manager_profile_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserCreationFlow);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_CREATION_FLOW_H_
