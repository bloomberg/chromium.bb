// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/locally_managed_user_creation_flow.h"

#include "base/values.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

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

void LocallyManagedUserCreationFlow::LaunchExtraSteps() {
  DictionaryValue* params = new DictionaryValue();
  params->SetString("user_display_name", name_);
  params->SetString("password", password_);

  BaseLoginDisplayHost::default_host()->
      StartWizard(WizardController::kLocallyManagedUserCreationScreenName,
          params);
}

}  // namespace chromeos
