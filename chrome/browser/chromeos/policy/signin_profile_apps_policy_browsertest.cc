// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Tests for the sign-in profile apps being enabled via the command line flag.
// TODO(emaxx): Remove this smoke test once it's investigated whether just
// specifying this command line flag leads to tests being timed out.
class SigninProfileAppsEnabledViaCommandLineTest : public InProcessBrowserTest {
 protected:
  SigninProfileAppsEnabledViaCommandLineTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableLoginScreenApps);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninProfileAppsEnabledViaCommandLineTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SigninProfileAppsEnabledViaCommandLineTest,
                       NoExtensions) {
  Profile* const initial_profile =
      g_browser_process->profile_manager()->GetProfileByPath(
          chromeos::ProfileHelper::GetSigninProfileDir());
  ASSERT_TRUE(initial_profile);
  EXPECT_TRUE(extensions::ExtensionSystem::Get(initial_profile)
                  ->extension_service()
                  ->extensions_enabled());
}

}  // namespace policy
