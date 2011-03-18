// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_IN_PROCESS_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_IN_PROCESS_BROWSER_TEST_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"

class Browser;
class Profile;
class WizardController;

namespace chromeos {

class LoginDisplayHost;

// Base class for test related to login wizard and its screens.
// Instead of creating Chrome browser window it creates login wizard window
// with specified parameters and allows to customize environment at the
// right moment in time before wizard is created.
class WizardInProcessBrowserTest : public CrosInProcessBrowserTest {
 public:
  explicit WizardInProcessBrowserTest(const char* screen_name);

 protected:
  // Can be overriden by derived test fixtures to set up environment after
  // browser is created but wizard is not shown yet.
  virtual void SetUpWizard() {}

  // Overriden from InProcessBrowserTest:
  virtual Browser* CreateBrowser(Profile* profile);
  virtual void CleanUpOnMainThread();

  WizardController* controller() const { return controller_; }
  void set_controller(WizardController* controller) {
    controller_ = controller;
  }

 private:
  std::string screen_name_;
  WizardController* controller_;
  LoginDisplayHost* host_;

  DISALLOW_COPY_AND_ASSIGN(WizardInProcessBrowserTest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WIZARD_IN_PROCESS_BROWSER_TEST_H_

