// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/options_ui_uitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"

namespace {

// Same as OptionsUITest but launches with Guest mode command line switches.
class GuestModeOptionsUITest : public OptionsUITest {
 public:
  GuestModeOptionsUITest() : OptionsUITest() {
    launch_arguments_.AppendSwitch(switches::kGuestSession);
    launch_arguments_.AppendSwitch(switches::kIncognito);
  }
};

// See bug 104393.
#if defined(USE_AURA)
#define MAYBE_LoadOptionsByURL FAILS_LoadOptionsByURL
#else
#define MAYBE_LoadOptionsByURL LoadOptionsByURL
#endif

TEST_F(GuestModeOptionsUITest, MAYBE_LoadOptionsByURL) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  scoped_refptr<TabProxy> tab = browser->GetActiveTab();
  ASSERT_TRUE(tab.get());

  NavigateToSettings(tab);
  VerifyTitle(tab);
  VerifyNavbar(tab);
  VerifySections(tab);
}

}  // namespace
