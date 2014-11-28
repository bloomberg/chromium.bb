// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/eula_model.h"

#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

const char EulaModel::kUserActionAcceptButtonClicked[] = "accept-button";
const char EulaModel::kUserActionBackButtonClicked[] = "back-button";
const char EulaModel::kContextKeyUsageStatsEnabled[] = "usageStatsEnabled";

EulaModel::EulaModel(BaseScreenDelegate* base_screen_delegate)
    : BaseScreen(base_screen_delegate) {
}

EulaModel::~EulaModel() {
}

std::string EulaModel::GetName() const {
  return WizardController::kEulaScreenName;
}

}  // namespace chromeos
