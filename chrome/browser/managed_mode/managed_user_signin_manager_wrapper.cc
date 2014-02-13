// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_signin_manager_wrapper.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_base.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_constants.h"
#endif

ManagedUserSigninManagerWrapper::ManagedUserSigninManagerWrapper(
    SigninManagerBase* original) : original_(original) {
}

ManagedUserSigninManagerWrapper::~ManagedUserSigninManagerWrapper() {
}

SigninManagerBase* ManagedUserSigninManagerWrapper::GetOriginal() {
  return original_;
}

std::string ManagedUserSigninManagerWrapper::GetEffectiveUsername() const {
  if (original_->profile()->IsManaged()) {
#if defined(ENABLE_MANAGED_USERS)
    DCHECK_EQ(std::string(), original_->GetAuthenticatedUsername());
    return managed_users::kManagedUserPseudoEmail;
#else
    NOTREACHED();
#endif
  }

  return original_->GetAuthenticatedUsername();
}

std::string ManagedUserSigninManagerWrapper::GetAccountIdToUse() const {
  if (original_->profile()->IsManaged()) {
#if defined(ENABLE_MANAGED_USERS)
    return managed_users::kManagedUserPseudoEmail;
#else
    NOTREACHED();
#endif
  }

  return original_->GetAuthenticatedAccountId();
}
