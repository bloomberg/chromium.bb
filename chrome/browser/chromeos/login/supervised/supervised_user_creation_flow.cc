// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/supervised/supervised_user_creation_flow.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_creation_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"

namespace chromeos {

namespace {

SupervisedUserCreationScreen* GetScreen(LoginDisplayHost* host) {
  DCHECK(host);
  DCHECK(host->GetWizardController());
  DCHECK(host->GetWizardController()->GetSupervisedUserCreationScreen());
  return host->GetWizardController()->GetSupervisedUserCreationScreen();
}

} // namespace

SupervisedUserCreationFlow::SupervisedUserCreationFlow(
    const std::string& manager_id)
        : ExtendedUserFlow(manager_id),
        token_validated_(false),
        logged_in_(false),
        session_started_(false),
        manager_profile_(NULL) {}

SupervisedUserCreationFlow::~SupervisedUserCreationFlow() {}

bool SupervisedUserCreationFlow::CanLockScreen() {
  return false;
}

bool SupervisedUserCreationFlow::ShouldShowSettings() {
  return false;
}

bool SupervisedUserCreationFlow::ShouldLaunchBrowser() {
  return false;
}

bool SupervisedUserCreationFlow::ShouldSkipPostLoginScreens() {
  return true;
}

void SupervisedUserCreationFlow::HandleOAuthTokenStatusChange(
    user_manager::User::OAuthTokenStatus status) {
  if (status == user_manager::User::OAUTH_TOKEN_STATUS_UNKNOWN)
    return;
  if (status == user_manager::User::OAUTH2_TOKEN_STATUS_INVALID) {
    GetScreen(host())->ShowManagerInconsistentStateErrorScreen();
    return;
  }
  DCHECK(status == user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
  // We expect that LaunchExtraSteps is called by this time (local
  // authentication happens before oauth token validation).
  token_validated_ = true;

  if (token_validated_ && logged_in_) {
    if (!session_started_)
      GetScreen(host())->OnManagerFullyAuthenticated(manager_profile_);
    session_started_ = true;
  }
}

bool SupervisedUserCreationFlow::HandleLoginFailure(
    const AuthFailure& failure) {
  if (failure.reason() == AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME)
    GetScreen(host())->OnManagerLoginFailure();
  else
    GetScreen(host())->ShowManagerInconsistentStateErrorScreen();
  return true;
}

void SupervisedUserCreationFlow::HandleLoginSuccess(
    const UserContext& context) {}

bool SupervisedUserCreationFlow::HandlePasswordChangeDetected() {
  GetScreen(host())->ShowManagerInconsistentStateErrorScreen();
  return true;
}

void SupervisedUserCreationFlow::LaunchExtraSteps(
    Profile* profile) {
  logged_in_ = true;
  manager_profile_ = profile;
  ProfileHelper::Get()->ProfileStartup(profile, true);

  if (token_validated_ && logged_in_) {
    if (!session_started_)
      GetScreen(host())->OnManagerFullyAuthenticated(manager_profile_);
    session_started_ = true;
  } else {
    GetScreen(host())->OnManagerCryptohomeAuthenticated();
  }
}

}  // namespace chromeos
