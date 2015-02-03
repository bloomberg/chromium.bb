// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/update_model.h"

#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

const char UpdateModel::kUserActionCancelUpdateShortcut[] = "cancel-update";
const char UpdateModel::kContextKeyEstimatedTimeLeftSec[] = "time-left-sec";
const char UpdateModel::kContextKeyShowEstimatedTimeLeft[] = "show-time-left";
const char UpdateModel::kContextKeyUpdateMessage[] = "update-msg";
const char UpdateModel::kContextKeyShowCurtain[] = "show-curtain";
const char UpdateModel::kContextKeyShowProgressMessage[] = "show-progress-msg";
const char UpdateModel::kContextKeyProgress[] = "progress";
const char UpdateModel::kContextKeyProgressMessage[] = "progress-msg";
const char UpdateModel::kContextKeyCancelUpdateShortcutEnabled[] =
    "cancel-update-enabled";

UpdateModel::UpdateModel(BaseScreenDelegate* base_screen_delegate)
    : BaseScreen(base_screen_delegate) {
}

UpdateModel::~UpdateModel() {
}

std::string UpdateModel::GetName() const {
  return WizardController::kUpdateScreenName;
}

}  // namespace chromeos
