// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"

#include "chrome/browser/profiles/profile.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "google_apis/gaia/gaia_constants.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#endif

SupervisedUserSigninManagerWrapper::SupervisedUserSigninManagerWrapper(
    Profile* profile,
    SigninManagerBase* original)
    : profile_(profile), original_(original) {}

SupervisedUserSigninManagerWrapper::~SupervisedUserSigninManagerWrapper() {
}

SigninManagerBase* SupervisedUserSigninManagerWrapper::GetOriginal() {
  return original_;
}

std::string SupervisedUserSigninManagerWrapper::GetEffectiveUsername() const {
  const std::string& auth_username = original_->GetAuthenticatedUsername();
#if defined(ENABLE_MANAGED_USERS)
  if (auth_username.empty() && profile_->IsSupervised())
    return supervised_users::kSupervisedUserPseudoEmail;
#endif
  return auth_username;
}

std::string SupervisedUserSigninManagerWrapper::GetAccountIdToUse() const {
  const std::string& auth_account = original_->GetAuthenticatedAccountId();
#if defined(ENABLE_MANAGED_USERS)
  if (auth_account.empty() && profile_->IsSupervised())
    return supervised_users::kSupervisedUserPseudoEmail;
#endif
  return auth_account;
}

std::string SupervisedUserSigninManagerWrapper::GetSyncScopeToUse() const {
#if defined(ENABLE_MANAGED_USERS)
  const std::string& auth_account = original_->GetAuthenticatedAccountId();
  if (auth_account.empty() && profile_->IsSupervised())
    return GaiaConstants::kChromeSyncSupervisedOAuth2Scope;
#endif
  return GaiaConstants::kChromeSyncOAuth2Scope;
}
