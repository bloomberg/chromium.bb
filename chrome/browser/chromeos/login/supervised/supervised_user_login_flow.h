// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_LOGIN_FLOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_LOGIN_FLOW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chromeos/login/auth/extended_authenticator.h"
#include "chromeos/login/auth/user_context.h"

namespace chromeos {

// UserFlow implementation for signing in supervised user.
class SupervisedUserLoginFlow
    : public ExtendedUserFlow,
      public ExtendedAuthenticator::NewAuthStatusConsumer {
 public:
  explicit SupervisedUserLoginFlow(const std::string& user_id);
  virtual ~SupervisedUserLoginFlow();

  // ExtendedUserFlow overrides.
  virtual bool CanLockScreen() OVERRIDE;
  virtual bool ShouldLaunchBrowser() OVERRIDE;
  virtual bool ShouldSkipPostLoginScreens() OVERRIDE;
  virtual bool HandleLoginFailure(const AuthFailure& failure) OVERRIDE;
  virtual void HandleLoginSuccess(const UserContext& context) OVERRIDE;
  virtual bool HandlePasswordChangeDetected() OVERRIDE;
  virtual void HandleOAuthTokenStatusChange(
      user_manager::User::OAuthTokenStatus status) OVERRIDE;
  virtual void LaunchExtraSteps(Profile* profile) OVERRIDE;

  // ExtendedAuthenticator::NewAuthStatusConsumer overrides.
  virtual void OnAuthenticationFailure(ExtendedAuthenticator::AuthState state)
      OVERRIDE;

 private:
  void Launch();
  void Finish();

  void OnSyncSetupDataLoaded(const std::string& token);
  void ConfigureSync(const std::string& token);
  void OnPasswordChangeDataLoaded(const base::DictionaryValue* password_data);
  void OnPasswordChangeDataLoadFailed();
  void OnNewKeyAdded(scoped_ptr<base::DictionaryValue> password_data);
  void OnOldKeyRemoved();
  void OnPasswordUpdated(scoped_ptr<base::DictionaryValue> password_data);

  scoped_refptr<ExtendedAuthenticator> authenticator_;

  bool data_loaded_;
  UserContext context_;
  Profile* profile_;
  base::WeakPtrFactory<SupervisedUserLoginFlow> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserLoginFlow);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_LOGIN_FLOW_H_
