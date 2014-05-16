// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_WIZARD_IN_PROCESS_BROWSER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_WIZARD_IN_PROCESS_BROWSER_TEST_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/test/base/in_process_browser_test.h"

class Browser;
class Profile;

namespace chromeos {

class LoginDisplayHost;
class WizardController;

// Base class for test related to login wizard and its screens.
// Instead of creating Chrome browser window it creates login wizard window
// with specified parameters and allows to customize environment at the
// right moment in time before wizard is created.
class WizardInProcessBrowserTest : public InProcessBrowserTest {
 public:
  explicit WizardInProcessBrowserTest(const char* screen_name);

  // Overridden from InProcessBrowserTest:
  virtual void SetUp() OVERRIDE;

 protected:
  // Can be overriden by derived test fixtures to set up environment after
  // browser is created but wizard is not shown yet.
  virtual void SetUpWizard() {}

  // Overriden from InProcessBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE;
  virtual void CleanUpOnMainThread() OVERRIDE;

 private:
  std::string screen_name_;
  LoginDisplayHost* host_;

  DISALLOW_COPY_AND_ASSIGN(WizardInProcessBrowserTest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_WIZARD_IN_PROCESS_BROWSER_TEST_H_
