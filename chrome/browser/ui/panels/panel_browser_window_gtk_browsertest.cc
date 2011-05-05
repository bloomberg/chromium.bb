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

class PanelBrowserWindowGtkTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnablePanels);
  }
};

IN_PROC_BROWSER_TEST_F(PanelBrowserWindowGtkTest, CreatePanel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  EXPECT_EQ(0, panel_manager->active_count()); // No panels initially.

  Browser* panel_browser = Browser::CreateForApp(Browser::TYPE_PANEL,
                                                 "PanelTest",
                                                 gfx::Size(),
                                                 browser()->profile());
  EXPECT_TRUE(panel_browser->is_type_panel());
  EXPECT_EQ(1, panel_manager->active_count());

  gfx::Rect bounds = panel_browser->window()->GetBounds();
  EXPECT_TRUE(bounds.x() > 0);
  EXPECT_TRUE(bounds.y() > 0);
  EXPECT_TRUE(bounds.width() > 0);
  EXPECT_TRUE(bounds.height() > 0);

  panel_browser->window()->Show();
  panel_browser->window()->Close();
  EXPECT_EQ(0, panel_manager->active_count());
}
