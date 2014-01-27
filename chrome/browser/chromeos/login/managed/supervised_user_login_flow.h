// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_SUPERVISED_USER_LOGIN_FLOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_SUPERVISED_USER_LOGIN_FLOW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/user_flow.h"

namespace chromeos {

// UserFlow implementation for signing in locally managed user.
class SupervisedUserLoginFlow : public ExtendedUserFlow {
 public:
  explicit SupervisedUserLoginFlow(const std::string& user_id);
  virtual ~SupervisedUserLoginFlow();

  // Registers flow preferences.
  //  static void RegisterPrefs(PrefRegistrySimple* registry);

  virtual bool CanLockScreen() OVERRIDE;
  virtual bool ShouldLaunchBrowser() OVERRIDE;
  virtual bool ShouldSkipPostLoginScreens() OVERRIDE;
  virtual bool HandleLoginFailure(const LoginFailure& failure) OVERRIDE;
  virtual bool HandlePasswordChangeDetected() OVERRIDE;
  virtual void HandleOAuthTokenStatusChange(User::OAuthTokenStatus status)
      OVERRIDE;
  virtual void LaunchExtraSteps(Profile* profile) OVERRIDE;

  virtual void OnSyncSetupDataLoaded(const std::string& token);
  virtual void ConfigureSync(const std::string& token);

 private:
  void Launch();

  bool data_loaded_;
  Profile* profile_;
  base::WeakPtrFactory<SupervisedUserLoginFlow> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserLoginFlow);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_SUPERVISED_USER_LOGIN_FLOW_H_
