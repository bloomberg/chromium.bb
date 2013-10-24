// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor.h"
#include "chrome/browser/signin/google_auto_login_helper.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"

// TODO(acleung): Should we flag protect all the operations for now?
AccountReconcilor::AccountReconcilor(Profile* profile) : profile_(profile) {
  ProfileOAuth2TokenService* o2ts =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  o2ts->AddObserver(this);
}

void AccountReconcilor::OnRefreshTokenAvailable(const std::string& account_id) {
  PerformMergeAction(account_id);
}

void AccountReconcilor::OnRefreshTokenRevoked(const std::string& account_id) {
  PerformRemoveAction(account_id);
}

void AccountReconcilor::OnRefreshTokensLoaded() {}

void AccountReconcilor::PerformMergeAction(const std::string& account_id) {
  // GoogleAutoLoginHelper deletes itself upon success / failure.
  GoogleAutoLoginHelper* helper = new GoogleAutoLoginHelper(profile_);
  helper->LogIn(account_id);
}

void AccountReconcilor::PerformRemoveAction(const std::string& account_id) {
  // TODO(acleung): Implement this:
}

void AccountReconcilor::PerformReconcileAction() {
  // TODO(acleung): Implement this:
}

AccountReconcilor::~AccountReconcilor() {}

void AccountReconcilor::Shutdown() {
  ProfileOAuth2TokenService* o2ts =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  o2ts->RemoveObserver(this);
}
