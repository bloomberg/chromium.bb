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
    string16 name,
    std::string password) : name_(name),
                            password_(password) {}

LocallyManagedUserCreationFlow::~LocallyManagedUserCreationFlow() {}

bool LocallyManagedUserCreationFlow::ShouldLaunchBrowser() {
  return false;
}

bool LocallyManagedUserCreationFlow::ShouldSkipPostLoginScreens() {
  return true;
}

bool LocallyManagedUserCreationFlow::HandleLoginFailure(
    const LoginFailure& failure,
    LoginDisplayHost* host) {
  if (failure.reason() == LoginFailure::COULD_NOT_MOUNT_CRYPTOHOME)
    GetScreen(host)->OnManagerLoginFailure();
  else
    GetScreen(host)->ShowManagerInconsistentStateErrorScreen();
  return true;
}

bool LocallyManagedUserCreationFlow::HandlePasswordChangeDetected(
    LoginDisplayHost* host) {
  GetScreen(host)->ShowManagerInconsistentStateErrorScreen();
  return true;
}

void LocallyManagedUserCreationFlow::LaunchExtraSteps(
    LoginDisplayHost* host) {
  GetScreen(host)->OnManagerSignIn();
}

}  // namespace chromeos
