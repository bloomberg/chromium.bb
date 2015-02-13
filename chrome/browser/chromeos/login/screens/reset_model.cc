// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/reset_model.h"

#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

const char ResetModel::kUserActionCancelReset[] = "cancel-reset";
const char ResetModel::kUserActionResetRestartPressed[] = "restart-pressed";
const char ResetModel::kUserActionResetPowerwashPressed[] = "powerwash-pressed";
const char ResetModel::kUserActionResetLearnMorePressed[] = "learn-more-link";
const char ResetModel::kUserActionResetRollbackToggled[] = "rollback-toggled";
const char ResetModel::kUserActionResetShowConfirmationPressed[] =
    "show-confirmation";
const char ResetModel::kUserActionResetResetConfirmationDismissed[] =
    "reset-confirm-dismissed";

const char ResetModel::kContextKeyIsRestartRequired[] = "restart-required";
const char ResetModel::kContextKeyIsRollbackAvailable[] = "rollback-available";
const char ResetModel::kContextKeyIsRollbackChecked[] = "rollback-checked";
const char ResetModel::kContextKeyIsConfirmational[] = "is-confirmational-view";
const char ResetModel::kContextKeyIsOfficialBuild[] = "is-official-build";
const char ResetModel::kContextKeyScreenState[] = "screen-state";

ResetModel::ResetModel(BaseScreenDelegate* base_screen_delegate)
    : BaseScreen(base_screen_delegate) {
}

ResetModel::~ResetModel() {
}

std::string ResetModel::GetName() const {
  return WizardController::kResetScreenName;
}

}  // namespace chromeos
