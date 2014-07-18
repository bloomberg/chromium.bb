// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_FACTORY_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class OobeDisplay;
class ScreenObserver;

// Class that can instantiate screens by name.
class ScreenFactory {
 public:
  static const char kEnrollmentScreenId[];
  static const char kErrorScreenId[];
  static const char kEulaScreenId[];
  static const char kKioskAutolaunchScreenId[];
  static const char kLoginScreenId[];
  static const char kNetworkScreenId[];
  static const char kResetScreenId[];
  static const char kSupervisedUserCreationScreenId[];
  static const char kTermsOfServiceScreenId[];
  static const char kUpdateScreenId[];
  static const char kUserImageScreenId[];
  static const char kWrongHWIDScreenId[];

  // |observer| to be passed to each created screen for providing screen
  // outcome. Legacy, should be gone once refactoring is finished.
  // |oobe_display| is a source for all the Handlers required for screens.
  ScreenFactory(ScreenObserver* observer,
                OobeDisplay* oobe_display);
  virtual ~ScreenFactory();

  // Create a screen given its |id|.
  BaseScreen* CreateScreen(const std::string& id);

 private:
  BaseScreen* CreateScreenImpl(const std::string& id);

  // Not owned. Screen observer for created screens. Legacy, should be gone
  // once refactoring is finished.
  ScreenObserver* observer_;

  // Now owned. Source of Handler objects for created screens.
  OobeDisplay* oobe_display_;

  DISALLOW_COPY_AND_ASSIGN(ScreenFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_FACTORY_H_
