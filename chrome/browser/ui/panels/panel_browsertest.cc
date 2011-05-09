// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class PanelBrowserTest : public InProcessBrowserTest {
 public:
  PanelBrowserTest() : InProcessBrowserTest() { }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnablePanels);
  }
};

// Panel is not supported for ChromeOS and Linux view yet.
// TODO(dimich): Turn on the test for MacOSX.
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && !defined(TOOLKIT_VIEWS)
IN_PROC_BROWSER_TEST_F(PanelBrowserTest, CreatePanel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  EXPECT_EQ(0, panel_manager->active_count()); // No panels initially.

  Browser* panel_browser = Browser::CreateForApp(Browser::TYPE_PANEL,
                                                 "PanelTest",
                                                 gfx::Size(),
                                                 browser()->profile());
  EXPECT_TRUE(panel_browser->is_type_panel());
  panel_browser->window()->Show();
  EXPECT_EQ(1, panel_manager->active_count());

  gfx::Rect bounds = panel_browser->window()->GetBounds();
  EXPECT_GT(bounds.x(), 0);
  EXPECT_GT(bounds.y(), 0);
  EXPECT_GT(bounds.width(), 0);
  EXPECT_GT(bounds.height(), 0);

  panel_browser->window()->Close();
  EXPECT_EQ(0, panel_manager->active_count());
}
#endif
