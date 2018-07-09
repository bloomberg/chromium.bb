// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEMO_PREFERENCES_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEMO_PREFERENCES_SCREEN_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class BaseScreenDelegate;
class DemoPreferencesScreenView;

// Controls demo mode preferences. The screen can be shown during OOBE. It
// allows user to choose preferences for retail demo mode.
class DemoPreferencesScreen : public BaseScreen {
 public:
  DemoPreferencesScreen(BaseScreenDelegate* base_screen_delegate,
                        DemoPreferencesScreenView* view);
  ~DemoPreferencesScreen() override;

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

  // Called when view is being destroyed. If Screen is destroyed earlier
  // then it has to call Bind(nullptr).
  void OnViewDestroyed(DemoPreferencesScreenView* view);

 private:
  DemoPreferencesScreenView* view_;

  DISALLOW_COPY_AND_ASSIGN(DemoPreferencesScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_DEMO_PREFERENCES_SCREEN_H_
