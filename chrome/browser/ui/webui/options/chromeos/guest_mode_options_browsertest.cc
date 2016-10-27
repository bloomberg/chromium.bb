// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/guest_mode_options_browsertest.h"

#include "base/command_line.h"
#include "chrome/browser/ui/webui/options/options_ui_browsertest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_names.h"

GuestModeOptionsUIBrowserTest::GuestModeOptionsUIBrowserTest() {}

GuestModeOptionsUIBrowserTest::~GuestModeOptionsUIBrowserTest() {}

void GuestModeOptionsUIBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(chromeos::switches::kGuestSession);
  command_line->AppendSwitchASCII(
      chromeos::switches::kLoginUser,
      user_manager::GuestAccountId().GetUserEmail());
  command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                  TestingProfile::kTestUserProfileDir);
  command_line->AppendSwitch(switches::kIncognito);
}
