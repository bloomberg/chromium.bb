// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/network_error_model.h"

#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

const char NetworkErrorModel::kContextKeyErrorStateCode[] = "error-state-code";
const char NetworkErrorModel::kContextKeyErrorStateNetwork[] =
    "error-state-network";
const char NetworkErrorModel::kContextKeyGuestSigninAllowed[] =
    "guest-signin-allowed";
const char NetworkErrorModel::kContextKeyOfflineSigninAllowed[] =
    "offline-signin-allowed";
const char NetworkErrorModel::kContextKeyShowConnectingIndicator[] =
    "show-connecting-indicator";
const char NetworkErrorModel::kContextKeyUIState[] = "ui-state";
const char NetworkErrorModel::kUserActionConfigureCertsButtonClicked[] =
    "configure-certs";
const char NetworkErrorModel::kUserActionDiagnoseButtonClicked[] = "diagnose";
const char NetworkErrorModel::kUserActionLaunchOobeGuestSessionClicked[] =
    "launch-oobe-guest";
const char
    NetworkErrorModel::kUserActionLocalStateErrorPowerwashButtonClicked[] =
        "local-state-error-powerwash";
const char NetworkErrorModel::kUserActionRebootButtonClicked[] = "reboot";
const char NetworkErrorModel::kUserActionShowCaptivePortalClicked[] =
    "show-captive-portal";
const char NetworkErrorModel::kUserActionConnectRequested[] =
    "connect-requested";

NetworkErrorModel::NetworkErrorModel(BaseScreenDelegate* base_screen_delegate)
    : BaseScreen(base_screen_delegate) {
}

NetworkErrorModel::~NetworkErrorModel() {
}

std::string NetworkErrorModel::GetName() const {
  return WizardController::kErrorScreenName;
}

}  // namespace chromeos
