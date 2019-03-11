// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEMO_PREFERENCES_SCREEN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEMO_PREFERENCES_SCREEN_VIEW_H_

#include <string>

#include "chrome/browser/chromeos/login/oobe_screen.h"

namespace chromeos {

class DemoPreferencesScreen;

// Interface of the demo mode preferences screen view.
class DemoPreferencesScreenView {
 public:
  constexpr static OobeScreen kScreenId =
      OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES;

  virtual ~DemoPreferencesScreenView();

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Sets view and screen.
  virtual void Bind(DemoPreferencesScreen* screen) = 0;

  // Called to set the input method id on JS side.
  virtual void SetInputMethodId(const std::string& input_method) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEMO_PREFERENCES_SCREEN_VIEW_H_
