// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_user_login_flow.h"

#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/easy_unlock_service.h"

EasyUnlockUserLoginFlow::EasyUnlockUserLoginFlow(const AccountId& account_id)
    : chromeos::ExtendedUserFlow(account_id) {}

EasyUnlockUserLoginFlow::~EasyUnlockUserLoginFlow() {}

bool EasyUnlockUserLoginFlow::CanLockScreen() {
  return true;
}

bool EasyUnlockUserLoginFlow::CanStartArc() {
  return true;
}

bool EasyUnlockUserLoginFlow::ShouldLaunchBrowser() {
  return true;
}

bool EasyUnlockUserLoginFlow::ShouldSkipPostLoginScreens() {
  return false;
}

bool EasyUnlockUserLoginFlow::HandleLoginFailure(
    const chromeos::AuthFailure& failure) {
  Profile* profile = chromeos::ProfileHelper::GetSigninProfile();
  EasyUnlockService* service = EasyUnlockService::Get(profile);
  if (!service)
    return false;
  service->HandleAuthFailure(account_id());
  service->RecordEasySignInOutcome(account_id(), false);
  UnregisterFlowSoon();
  return true;
}

void EasyUnlockUserLoginFlow::HandleLoginSuccess(
    const chromeos::UserContext& context) {
  Profile* profile = chromeos::ProfileHelper::GetSigninProfile();
  EasyUnlockService* service = EasyUnlockService::Get(profile);
  if (!service)
    return;
  service->RecordEasySignInOutcome(account_id(), true);
}

bool EasyUnlockUserLoginFlow::HandlePasswordChangeDetected() {
  return false;
}

void EasyUnlockUserLoginFlow::HandleOAuthTokenStatusChange(
    user_manager::User::OAuthTokenStatus status) {
}

void EasyUnlockUserLoginFlow::LaunchExtraSteps(Profile* profile) {
}

bool EasyUnlockUserLoginFlow::SupportsEarlyRestartToApplyFlags() {
  return true;
}
