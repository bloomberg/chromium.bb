// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileSigninConfirmationHelperBrowserTest : public InProcessBrowserTest {
 public:
  ProfileSigninConfirmationHelperBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Force the first-run flow to trigger autoimport.
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kForceFirstRun);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileSigninConfirmationHelperBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ProfileSigninConfirmationHelperBrowserTest,
                       HasNotBeenShutdown) {
#if !defined(OS_CHROMEOS)
  EXPECT_TRUE(first_run::auto_import_state() & first_run::AUTO_IMPORT_CALLED);
#endif
  EXPECT_FALSE(ui::HasBeenShutdown(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ProfileSigninConfirmationHelperBrowserTest,
                       HasNoSyncedExtensions) {
  EXPECT_FALSE(ui::HasSyncedExtensions(browser()->profile()));
}
