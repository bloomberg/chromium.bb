// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/login/account_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace chromeos {

class AccountScreenTest : public InProcessBrowserTest {
 public:
  AccountScreenTest() {
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchWithValue(switches::kLoginScreen, L"account");
    command_line->AppendSwitchWithValue(switches::kLoginScreenSize,
                                        L"1024,600");
  }

  virtual Browser* CreateBrowser(Profile* profile) {
    return NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountScreenTest);
};

IN_PROC_BROWSER_TEST_F(AccountScreenTest, DISABLED_TestBasic) {
  WizardController* controller = WizardController::default_controller();
  ASSERT_TRUE(NULL != controller);
  EXPECT_EQ(controller->GetAccountScreen(), controller->current_screen());
  // Close login manager windows.
  MessageLoop::current()->DeleteSoon(FROM_HERE, controller);
  // End the message loop to quit the test since there's no browser window
  // created.
  MessageLoop::current()->Quit();
}

}  // namespace chromeos
