// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CREATION_FLOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CREATION_FLOW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/login/user_flow.h"

namespace chromeos {

// UserFlow implementation for creating new locally managed user.
class LocallyManagedUserCreationFlow : public ExtendedUserFlow {
 public:
  LocallyManagedUserCreationFlow(const std::string& manager_id,
                                 string16 name,
                                 const std::string& password);
  virtual ~LocallyManagedUserCreationFlow();

  virtual bool ShouldLaunchBrowser() OVERRIDE;
  virtual bool ShouldSkipPostLoginScreens() OVERRIDE;
  virtual bool HandleLoginFailure(const LoginFailure& failure) OVERRIDE;
  virtual bool HandlePasswordChangeDetected() OVERRIDE;
  virtual void HandleOAuthTokenStatusChange(User::OAuthTokenStatus status)
      OVERRIDE;
  virtual void LaunchExtraSteps(Profile* profile) OVERRIDE;
 private:
  // Display name for user being created.
  string16 name_;
  // Password for user being created.
  std::string password_;

  // Indicates if manager OAuth2 token has been validated.
  bool token_validated_;

  // Indicates if manager was successfully authenticated against
  // local cryptohome.
  bool logged_in_;

  DISALLOW_COPY_AND_ASSIGN(LocallyManagedUserCreationFlow);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CREATION_FLOW_H_
