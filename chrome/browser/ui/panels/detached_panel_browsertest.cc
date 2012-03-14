// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/detached_panel_strip.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"

class DetachedPanelBrowserTest : public BasePanelBrowserTest {
};

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest, CheckDetachedPanelProperties) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelStrip* detached_strip = panel_manager->detached_strip();

  // Create 2 panels.
  Panel* panel1 = CreateDetachedPanel("1", gfx::Rect(300, 200, 250, 200));
  Panel* panel2 = CreateDetachedPanel("2", gfx::Rect(350, 180, 300, 200));

  EXPECT_EQ(2, panel_manager->num_panels());
  EXPECT_EQ(2, detached_strip->num_panels());

  EXPECT_TRUE(detached_strip->HasPanel(panel1));
  EXPECT_TRUE(detached_strip->HasPanel(panel2));

  EXPECT_TRUE(panel1->draggable());
  EXPECT_TRUE(panel2->draggable());

  panel_manager->CloseAll();
}
