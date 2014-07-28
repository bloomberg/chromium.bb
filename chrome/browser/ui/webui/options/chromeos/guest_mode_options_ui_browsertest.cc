// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/webui/options/options_ui_browsertest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/user_names.h"

namespace {

// Same as OptionsUIBrowserTest but launches with Guest mode command line
// switches.
class GuestModeOptionsBrowserTest : public options::OptionsUIBrowserTest {
 public:
  GuestModeOptionsBrowserTest() : OptionsUIBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kGuestSession);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                    chromeos::login::kGuestUserName);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
    command_line->AppendSwitch(switches::kIncognito);
  }
};

IN_PROC_BROWSER_TEST_F(GuestModeOptionsBrowserTest, LoadOptionsByURL) {
  NavigateToSettings();
  VerifyTitle();
  VerifyNavbar();
}

}  // namespace
