// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/detached_panel_strip.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"

class DetachedPanelBrowserTest : public BasePanelBrowserTest {
};

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest,
                       DISABLED_CheckDetachedPanelProperties) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelStrip* detached_strip = panel_manager->detached_strip();

  // Create 2 panels.
  Panel* panel1 = CreatePanelWithBounds("Panel1", gfx::Rect(0, 0, 250, 200));
  Panel* panel2 = CreatePanelWithBounds("Panel2", gfx::Rect(0, 0, 300, 200));

  EXPECT_TRUE(panel1->draggable());
  EXPECT_TRUE(panel2->draggable());

  // Move panels to detached strip.
  EXPECT_EQ(2, panel_manager->num_panels());
  EXPECT_EQ(0, detached_strip->num_panels());
  panel1->MoveToStrip(detached_strip);
  panel2->MoveToStrip(detached_strip);
  EXPECT_EQ(2, panel_manager->num_panels());
  EXPECT_EQ(2, detached_strip->num_panels());

  std::vector<Panel*> panels = panel_manager->panels();
  EXPECT_EQ(panel1, panels[0]);
  EXPECT_EQ(panel2, panels[1]);

  EXPECT_TRUE(panel1->draggable());
  EXPECT_TRUE(panel2->draggable());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest, DragDetachedPanel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelStrip* detached_strip = panel_manager->detached_strip();

  // Create one detached panel.
  Panel* panel = CreatePanelWithBounds("Panel1", gfx::Rect(0, 0, 250, 200));
  panel->MoveToStrip(detached_strip);

  // Test that the detached panel can be dragged anywhere.
  scoped_ptr<NativePanelTesting> panel_testing(
      NativePanelTesting::Create(panel->native_panel()));
  gfx::Point origin = panel->GetBounds().origin();

  panel_testing->PressLeftMouseButtonTitlebar(origin);
  EXPECT_EQ(origin, panel->GetBounds().origin());

  panel_testing->DragTitlebar(-51, 102);
  origin.Offset(-51, 102);
  EXPECT_EQ(origin, panel->GetBounds().origin());

  panel_testing->DragTitlebar(37, -42);
  origin.Offset(37, -42);
  EXPECT_EQ(origin, panel->GetBounds().origin());

  panel_testing->FinishDragTitlebar();
  EXPECT_EQ(origin, panel->GetBounds().origin());

  // Test that cancelling the drag will return the panel the the original
  // position.
  gfx::Point original_position = panel->GetBounds().origin();
  origin = original_position;

  panel_testing->PressLeftMouseButtonTitlebar(origin);
  EXPECT_EQ(origin, panel->GetBounds().origin());

  panel_testing->DragTitlebar(-51, 102);
  origin.Offset(-51, 102);
  EXPECT_EQ(origin, panel->GetBounds().origin());

  panel_testing->DragTitlebar(37, -42);
  origin.Offset(37, -42);
  EXPECT_EQ(origin, panel->GetBounds().origin());

  panel_testing->CancelDragTitlebar();
  EXPECT_EQ(original_position, panel->GetBounds().origin());

  panel_manager->CloseAll();
}
