// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/network_model.h"

#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

const char NetworkModel::kUserActionContinueButtonClicked[] = "continue";
const char NetworkModel::kUserActionConnectDebuggingFeaturesClicked[] =
    "connect-debugging-features";
const char NetworkModel::kContextKeyLocale[] = "locale";
const char NetworkModel::kContextKeyInputMethod[] = "input-method";
const char NetworkModel::kContextKeyTimezone[] = "timezone";
const char NetworkModel::kContextKeyContinueButtonEnabled[] =
    "continue-button-enabled";

NetworkModel::NetworkModel(BaseScreenDelegate* base_screen_delegate)
    : BaseScreen(base_screen_delegate) {
}

NetworkModel::~NetworkModel() {
}

std::string NetworkModel::GetName() const {
  return WizardController::kNetworkScreenName;
}

}  // namespace chromeos
