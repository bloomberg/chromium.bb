// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/signin_manager_wrapper.h"

#include "components/signin/core/browser/signin_manager_base.h"
#include "google_apis/gaia/gaia_constants.h"

SigninManagerWrapper::SigninManagerWrapper(SigninManagerBase* original)
    : original_(original) {}

SigninManagerWrapper::~SigninManagerWrapper() {}

SigninManagerBase* SigninManagerWrapper::GetOriginal() {
  return original_;
}

std::string SigninManagerWrapper::GetEffectiveUsername() const {
  return original_->GetAuthenticatedAccountInfo().email;
}

std::string SigninManagerWrapper::GetAccountIdToUse() const {
  return original_->GetAuthenticatedAccountId();
}

std::string SigninManagerWrapper::GetSyncScopeToUse() const {
  return GaiaConstants::kChromeSyncOAuth2Scope;
}
