// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/hid_detection_model.h"

#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

const char HIDDetectionModel::kContextKeyKeyboardState[] = "keyboard-state";
const char HIDDetectionModel::kContextKeyMouseState[] = "mouse-state";
const char HIDDetectionModel::kContextKeyPinCode[] = "keyboard-pincode";
const char HIDDetectionModel::kContextKeyNumKeysEnteredExpected[] =
    "num-keys-entered-expected";
const char HIDDetectionModel::kContextKeyNumKeysEnteredPinCode[] =
    "num-keys-entered-pincode";
const char HIDDetectionModel::kContextKeyMouseDeviceName[] =
    "mouse-device-name";
const char HIDDetectionModel::kContextKeyKeyboardDeviceName[] =
    "keyboard-device-name";
const char HIDDetectionModel::kContextKeyKeyboardLabel[] =
    "keyboard-device-label";
const char HIDDetectionModel::kContextKeyContinueButtonEnabled[] =
    "continue-button-enabled";

HIDDetectionModel::HIDDetectionModel(BaseScreenDelegate* base_screen_delegate)
    : BaseScreen(base_screen_delegate) {
}

HIDDetectionModel::~HIDDetectionModel() {
}

std::string HIDDetectionModel::GetName() const {
  return WizardController::kHIDDetectionScreenName;
}

}  // namespace chromeos
