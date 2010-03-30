// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/account_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/test/in_process_browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class AccountScreenTest : public WizardInProcessBrowserTest {
 public:
  AccountScreenTest(): WizardInProcessBrowserTest("account") {
  }

 protected:
  // Overriden from WizardInProcessBrowserTest:
  virtual void SetUpWizard() {
    HTTPTestServer* server = StartHTTPServer();
    ASSERT_TRUE(server != NULL);
    GURL new_account_page_url(server->TestServerPage("new_account.html"));
    AccountScreen::set_new_account_page_url(new_account_page_url);
    AccountScreen::set_check_for_https(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountScreenTest);
};

IN_PROC_BROWSER_TEST_F(AccountScreenTest, TestBasic) {
  ASSERT_TRUE(controller());
  EXPECT_EQ(controller()->GetAccountScreen(), controller()->current_screen());
}

}  // namespace chromeos
