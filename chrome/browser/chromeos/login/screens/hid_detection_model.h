// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HID_DETECTION_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HID_DETECTION_MODEL_H_

#include "base/callback_forward.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class BaseScreenDelegate;
class HIDDetectionView;

class HIDDetectionModel : public BaseScreen {
 public:
  static const char kContextKeyKeyboardState[];
  static const char kContextKeyMouseState[];
  static const char kContextKeyNumKeysEnteredExpected[];
  static const char kContextKeyNumKeysEnteredPinCode[];
  static const char kContextKeyPinCode[];
  static const char kContextKeyMouseDeviceName[];
  static const char kContextKeyKeyboardDeviceName[];
  static const char kContextKeyKeyboardLabel[];
  static const char kContextKeyContinueButtonEnabled[];

  explicit HIDDetectionModel(BaseScreenDelegate* base_screen_delegate);
  ~HIDDetectionModel() override;

  // BaseScreen implementation:
  std::string GetName() const override;

  // Called when continue button was clicked.
  virtual void OnContinueButtonClicked() = 0;

  // Checks if we should show the screen or enough devices already present.
  // Calls corresponding set of actions based on the bool result.
  virtual void CheckIsScreenRequired(
      const base::Callback<void(bool)>& on_check_done) = 0;

  // This method is called, when view is being destroyed. Note, if model
  // is destroyed earlier then it has to call Unbind().
  virtual void OnViewDestroyed(HIDDetectionView* view) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HID_DETECTION_MODEL_H_
