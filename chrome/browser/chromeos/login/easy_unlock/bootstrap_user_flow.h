// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_BOOTSTRAP_USER_FLOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_BOOTSTRAP_USER_FLOW_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chromeos/login/auth/extended_authenticator.h"
#include "chromeos/login/auth/user_context.h"

class Profile;

namespace chromeos {

// BootstrapUserFlow executes post cryptohome mount bootstraping tasks:
// 1. Performs Easy unlock pairing with the user's phone;
// 2. Creates easy unlock cryptohome keys;
// 3. Drops temp random cryptohome key;
// After these tasks, it finishes and resumes user session.
class BootstrapUserFlow final
    : public ExtendedUserFlow,
      public ExtendedAuthenticator::NewAuthStatusConsumer {
 public:
  // Constructs a BootstrapUserFlow. |user_context| is the user this flow
  // represents and contains the user id and key needed for cryptohome
  // operations. |is_new_account| is a boolean flag of whether the user
  // is being added to the device.
  BootstrapUserFlow(const UserContext& user_context, bool is_new_account);
  ~BootstrapUserFlow() override;

 private:
  void StartAutoPairing();
  void SetAutoPairingResult(bool success, const std::string& error_message);

  void OnKeysRefreshed(bool success);
  void RemoveBootstrapRandomKey();
  void OnBootstrapRandomKeyRemoved();
  void Finish();

  // chromeos::ExtendedUserFlow
  bool CanLockScreen() override;
  bool CanStartArc() override;
  bool ShouldLaunchBrowser() override;
  bool ShouldSkipPostLoginScreens() override;
  bool HandleLoginFailure(const chromeos::AuthFailure& failure) override;
  void HandleLoginSuccess(const chromeos::UserContext& context) override;
  bool HandlePasswordChangeDetected() override;
  void HandleOAuthTokenStatusChange(
      user_manager::User::OAuthTokenStatus status) override;
  void LaunchExtraSteps(Profile* user_profile) override;
  bool SupportsEarlyRestartToApplyFlags() override;

  // ExtendedAuthenticator::NewAuthStatusConsumer
  void OnAuthenticationFailure(ExtendedAuthenticator::AuthState state) override;

  UserContext user_context_;
  const bool is_new_account_;

  bool finished_;
  Profile* user_profile_;

  base::WeakPtrFactory<BootstrapUserFlow> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BootstrapUserFlow);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_BOOTSTRAP_USER_FLOW_H_
