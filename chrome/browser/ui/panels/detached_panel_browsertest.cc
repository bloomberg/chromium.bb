// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/detached_panel_strip.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_drag_controller.h"
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

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest, DragDetachedPanel) {
  PanelManager* panel_manager = PanelManager::GetInstance();

  Panel* panel = CreateDetachedPanel("1", gfx::Rect(300, 200, 250, 200));

  // Test that the detached panel can be dragged almost anywhere except getting
  // close to the bottom of the docked area to trigger the attach.
  scoped_ptr<NativePanelTesting> panel_testing(
      NativePanelTesting::Create(panel->native_panel()));
  gfx::Point origin = panel->GetBounds().origin();

  panel_testing->PressLeftMouseButtonTitlebar(origin);
  EXPECT_EQ(origin, panel->GetBounds().origin());

  origin.Offset(-51, -102);
  panel_testing->DragTitlebar(origin);
  EXPECT_EQ(origin, panel->GetBounds().origin());

  origin.Offset(37, 45);
  panel_testing->DragTitlebar(origin);
  EXPECT_EQ(origin, panel->GetBounds().origin());

  panel_testing->FinishDragTitlebar();
  EXPECT_EQ(origin, panel->GetBounds().origin());

  // Test that cancelling the drag will return the panel the the original
  // position.
  gfx::Point original_position = panel->GetBounds().origin();
  origin = original_position;

  panel_testing->PressLeftMouseButtonTitlebar(origin);
  EXPECT_EQ(origin, panel->GetBounds().origin());

  origin.Offset(-51, -102);
  panel_testing->DragTitlebar(origin);
  EXPECT_EQ(origin, panel->GetBounds().origin());

  origin.Offset(37, 45);
  panel_testing->DragTitlebar(origin);
  EXPECT_EQ(origin, panel->GetBounds().origin());

  panel_testing->CancelDragTitlebar();
  WaitForBoundsAnimationFinished(panel);
  EXPECT_EQ(original_position, panel->GetBounds().origin());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(DetachedPanelBrowserTest, CloseDetachedPanelOnDrag) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  PanelDragController* drag_controller = panel_manager->drag_controller();
  DetachedPanelStrip* detached_strip = panel_manager->detached_strip();

  // Create 4 detached panels.
  Panel* panel1 = CreateDetachedPanel("1", gfx::Rect(100, 200, 100, 100));
  Panel* panel2 = CreateDetachedPanel("2", gfx::Rect(200, 210, 110, 110));
  Panel* panel3 = CreateDetachedPanel("3", gfx::Rect(300, 220, 120, 120));
  Panel* panel4 = CreateDetachedPanel("4", gfx::Rect(400, 230, 130, 130));
  ASSERT_EQ(4, detached_strip->num_panels());

  scoped_ptr<NativePanelTesting> panel1_testing(
      NativePanelTesting::Create(panel1->native_panel()));
  gfx::Point panel1_old_position = panel1->GetBounds().origin();
  gfx::Point panel2_position = panel2->GetBounds().origin();
  gfx::Point panel3_position = panel3->GetBounds().origin();
  gfx::Point panel4_position = panel4->GetBounds().origin();

  // Test the scenario: drag a panel, close another panel, cancel the drag.
  {
    gfx::Point panel1_new_position = panel1_old_position;
    panel1_new_position.Offset(-51, -102);

    // Start dragging a panel.
    panel1_testing->PressLeftMouseButtonTitlebar(panel1->GetBounds().origin());
    panel1_testing->DragTitlebar(panel1_new_position);
    EXPECT_TRUE(drag_controller->IsDragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());

    ASSERT_EQ(4, detached_strip->num_panels());
    EXPECT_TRUE(detached_strip->HasPanel(panel1));
    EXPECT_TRUE(detached_strip->HasPanel(panel2));
    EXPECT_TRUE(detached_strip->HasPanel(panel3));
    EXPECT_TRUE(detached_strip->HasPanel(panel4));
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());
    EXPECT_EQ(panel2_position, panel2->GetBounds().origin());
    EXPECT_EQ(panel3_position, panel3->GetBounds().origin());
    EXPECT_EQ(panel4_position, panel4->GetBounds().origin());

    // Closing another panel while dragging in progress will keep the dragging
    // panel intact.
    CloseWindowAndWait(panel2->browser());
    EXPECT_TRUE(drag_controller->IsDragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());

    ASSERT_EQ(3, detached_strip->num_panels());
    EXPECT_TRUE(detached_strip->HasPanel(panel1));
    EXPECT_TRUE(detached_strip->HasPanel(panel3));
    EXPECT_TRUE(detached_strip->HasPanel(panel4));
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());
    EXPECT_EQ(panel3_position, panel3->GetBounds().origin());
    EXPECT_EQ(panel4_position, panel4->GetBounds().origin());

    // Cancel the drag.
    panel1_testing->CancelDragTitlebar();
    EXPECT_FALSE(drag_controller->IsDragging());

    ASSERT_EQ(3, detached_strip->num_panels());
    EXPECT_TRUE(detached_strip->HasPanel(panel1));
    EXPECT_TRUE(detached_strip->HasPanel(panel3));
    EXPECT_TRUE(detached_strip->HasPanel(panel4));
    EXPECT_EQ(panel1_old_position, panel1->GetBounds().origin());
    EXPECT_EQ(panel3_position, panel3->GetBounds().origin());
    EXPECT_EQ(panel4_position, panel4->GetBounds().origin());
  }

  // Test the scenario: drag a panel, close another panel, end the drag.
  {
    gfx::Point panel1_new_position = panel1_old_position;
    panel1_new_position.Offset(-51, -102);

    // Start dragging a panel.
    panel1_testing->PressLeftMouseButtonTitlebar(panel1->GetBounds().origin());
    panel1_testing->DragTitlebar(panel1_new_position);
    EXPECT_TRUE(drag_controller->IsDragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());

    ASSERT_EQ(3, detached_strip->num_panels());
    EXPECT_TRUE(detached_strip->HasPanel(panel1));
    EXPECT_TRUE(detached_strip->HasPanel(panel3));
    EXPECT_TRUE(detached_strip->HasPanel(panel4));
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());
    EXPECT_EQ(panel3_position, panel3->GetBounds().origin());
    EXPECT_EQ(panel4_position, panel4->GetBounds().origin());

    // Closing another panel while dragging in progress will keep the dragging
    // panel intact.
    CloseWindowAndWait(panel3->browser());
    EXPECT_TRUE(drag_controller->IsDragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());

    ASSERT_EQ(2, detached_strip->num_panels());
    EXPECT_TRUE(detached_strip->HasPanel(panel1));
    EXPECT_TRUE(detached_strip->HasPanel(panel4));
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());
    EXPECT_EQ(panel4_position, panel4->GetBounds().origin());

    // Finish the drag.
    panel1_testing->FinishDragTitlebar();
    EXPECT_FALSE(drag_controller->IsDragging());

    ASSERT_EQ(2, detached_strip->num_panels());
    EXPECT_TRUE(detached_strip->HasPanel(panel1));
    EXPECT_TRUE(detached_strip->HasPanel(panel4));
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());
    EXPECT_EQ(panel4_position, panel4->GetBounds().origin());
  }

  // Test the scenario: drag a panel and close the dragging panel.
  {
    gfx::Point panel1_new_position = panel1->GetBounds().origin();
    panel1_new_position.Offset(-51, -102);

    // Start dragging a panel again.
    panel1_testing->PressLeftMouseButtonTitlebar(panel1->GetBounds().origin());
    panel1_testing->DragTitlebar(panel1_new_position);
    EXPECT_TRUE(drag_controller->IsDragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());

    ASSERT_EQ(2, detached_strip->num_panels());
    EXPECT_TRUE(detached_strip->HasPanel(panel1));
    EXPECT_TRUE(detached_strip->HasPanel(panel4));
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());
    EXPECT_EQ(panel4_position, panel4->GetBounds().origin());

    // Closing the dragging panel should end the drag.
    CloseWindowAndWait(panel1->browser());
    EXPECT_FALSE(drag_controller->IsDragging());

    ASSERT_EQ(1, detached_strip->num_panels());
    EXPECT_TRUE(detached_strip->HasPanel(panel4));
    EXPECT_EQ(panel4_position, panel4->GetBounds().origin());
  }

  panel_manager->CloseAll();
}
