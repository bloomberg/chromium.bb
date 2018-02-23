// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/features.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "google_apis/gaia/gaia_constants.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#endif

SupervisedUserSigninManagerWrapper::SupervisedUserSigninManagerWrapper(
    Profile* profile,
    SigninManagerBase* original)
    : SigninManagerWrapper(original), profile_(profile) {}

SupervisedUserSigninManagerWrapper::~SupervisedUserSigninManagerWrapper() {}

std::string SupervisedUserSigninManagerWrapper::GetEffectiveUsername() const {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (profile_->IsLegacySupervised())
    return supervised_users::kSupervisedUserPseudoEmail;
#endif
  return SigninManagerWrapper::GetEffectiveUsername();
}

std::string SupervisedUserSigninManagerWrapper::GetAccountIdToUse() const {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (profile_->IsLegacySupervised())
    return supervised_users::kSupervisedUserPseudoEmail;
#endif
  return SigninManagerWrapper::GetAccountIdToUse();
}

std::string SupervisedUserSigninManagerWrapper::GetSyncScopeToUse() const {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (profile_->IsLegacySupervised())
    return GaiaConstants::kChromeSyncSupervisedOAuth2Scope;
#endif
  return SigninManagerWrapper::GetSyncScopeToUse();
}
