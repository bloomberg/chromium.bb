// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WIZARD_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WIZARD_SCREEN_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class ScreenObserver;

// Base class for the OOBE screens.
class WizardScreen : public BaseScreen {
 public:
  explicit WizardScreen(ScreenObserver* screen_observer);
  virtual ~WizardScreen() {}

 protected:
  ScreenObserver* get_screen_observer() const {
    return screen_observer_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(EnrollmentScreenTest, TestCancel);
  FRIEND_TEST_ALL_PREFIXES(EnrollmentScreenTest, TestSuccess);
  FRIEND_TEST_ALL_PREFIXES(ProvisionedEnrollmentScreenTest, TestBackButton);
  friend class NetworkScreenTest;
  friend class UpdateScreenTest;

  ScreenObserver* screen_observer_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WIZARD_SCREEN_H_
