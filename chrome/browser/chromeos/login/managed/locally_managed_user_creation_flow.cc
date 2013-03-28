// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/locally_managed_user_creation_flow.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_creation_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

namespace {

LocallyManagedUserCreationScreen* GetScreen(LoginDisplayHost* host) {
  DCHECK(host);
  DCHECK(host->GetWizardController());
  DCHECK(host->GetWizardController()->GetLocallyManagedUserCreationScreen());
  return host->GetWizardController()->GetLocallyManagedUserCreationScreen();
}

} // namespace

LocallyManagedUserCreationFlow::LocallyManagedUserCreationFlow(
    const std::string& manager_id,
    string16 name,
    const std::string& password) : ExtendedUserFlow(manager_id),
                            name_(name),
                            password_(password),
                            token_validated_(false),
                            logged_in_(false) {}

LocallyManagedUserCreationFlow::~LocallyManagedUserCreationFlow() {}

bool LocallyManagedUserCreationFlow::ShouldLaunchBrowser() {
  return false;
}

bool LocallyManagedUserCreationFlow::ShouldSkipPostLoginScreens() {
  return true;
}

void LocallyManagedUserCreationFlow::HandleOAuthTokenStatusChange(
    User::OAuthTokenStatus status) {
  if (status == User::OAUTH_TOKEN_STATUS_UNKNOWN)
    return;
  if (status == User::OAUTH2_TOKEN_STATUS_INVALID) {
    GetScreen(host())->ShowManagerInconsistentStateErrorScreen();
    return;
  }
  DCHECK(status == User::OAUTH2_TOKEN_STATUS_VALID);
  // We expect that LaunchExtraSteps is called by this time (local
  // authentication happens before oauth token validation).
  token_validated_ = true;

  if (token_validated_ && logged_in_)
    GetScreen(host())->OnManagerSignIn();
}


bool LocallyManagedUserCreationFlow::HandleLoginFailure(
    const LoginFailure& failure) {
  if (failure.reason() == LoginFailure::COULD_NOT_MOUNT_CRYPTOHOME)
    GetScreen(host())->OnManagerLoginFailure();
  else
    GetScreen(host())->ShowManagerInconsistentStateErrorScreen();
  return true;
}

bool LocallyManagedUserCreationFlow::HandlePasswordChangeDetected() {
  GetScreen(host())->ShowManagerInconsistentStateErrorScreen();
  return true;
}

void LocallyManagedUserCreationFlow::LaunchExtraSteps(
    Profile* profile) {

  logged_in_ = true;
  if (token_validated_ && logged_in_)
    GetScreen(host())->OnManagerSignIn();
}

}  // namespace chromeos
