// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class PanelBrowserTest : public InProcessBrowserTest {
 public:
  PanelBrowserTest() : InProcessBrowserTest() { }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnablePanels);
  }
};

// Panel is now only supported on windows.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, CreatePanel) {
  Browser* panel = Browser::CreateForApp(Browser::TYPE_PANEL, "PanelTest",
                                         gfx::Size(), browser()->profile());
  EXPECT_EQ(Browser::TYPE_PANEL, panel->type());
  panel->window()->Show();
  panel->window()->Close();
}
#endif
