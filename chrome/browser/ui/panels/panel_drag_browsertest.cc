// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/detached_panel_collection.h"
#include "chrome/browser/ui/panels/docked_panel_collection.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_drag_controller.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "chrome/browser/ui/panels/test_panel_collection_squeeze_observer.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

class PanelDragBrowserTest : public BasePanelBrowserTest {
 public:
  PanelDragBrowserTest() : BasePanelBrowserTest() {
  }

  virtual ~PanelDragBrowserTest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    BasePanelBrowserTest::SetUpOnMainThread();

    // All the tests here assume using mocked 800x600 screen area for the
    // primary monitor. Do the check now.
    gfx::Rect primary_screen_area = PanelManager::GetInstance()->
        display_settings_provider()->GetPrimaryScreenArea();
    DCHECK(primary_screen_area.width() == 800);
    DCHECK(primary_screen_area.height() == 600);
  }

  // Drag |panel| from its origin by the offset |delta|.
  void DragPanelByDelta(Panel* panel, const gfx::Vector2d& delta) {
    scoped_ptr<NativePanelTesting> panel_testing(
        CreateNativePanelTesting(panel));
    gfx::Point mouse_location(panel->GetBounds().origin());
    panel_testing->PressLeftMouseButtonTitlebar(mouse_location);
    panel_testing->DragTitlebar(mouse_location + delta);
    panel_testing->FinishDragTitlebar();
  }

  // Drag |panel| from its origin to |new_mouse_location|.
  void DragPanelToMouseLocation(Panel* panel,
                                const gfx::Point& new_mouse_location) {
    scoped_ptr<NativePanelTesting> panel_testing(
        CreateNativePanelTesting(panel));
    gfx::Point mouse_location(panel->GetBounds().origin());
    panel_testing->PressLeftMouseButtonTitlebar(panel->GetBounds().origin());
    panel_testing->DragTitlebar(new_mouse_location);
    panel_testing->FinishDragTitlebar();
  }

  // Return the bounds of a panel given its initial bounds and the bounds of the
  // panel above it.
  static gfx::Rect GetStackedAtBottomPanelBounds(
      const gfx::Rect& initial_bounds,
      const gfx::Rect& above_bounds) {
    return gfx::Rect(above_bounds.x(),
                     above_bounds.bottom(),
                     above_bounds.width(),
                     initial_bounds.height());
  }

  // Return the bounds of a panel given its initial bounds and the bounds of the
  // panel below it.
  static gfx::Rect GetStackedAtTopPanelBounds(
      const gfx::Rect& initial_bounds,
      const gfx::Rect& below_bounds) {
    return gfx::Rect(below_bounds.x(),
                     below_bounds.y() - initial_bounds.height(),
                     initial_bounds.width(),
                     initial_bounds.height());
  }

  static gfx::Vector2d GetDragDeltaToRemainDocked() {
    return gfx::Vector2d(
        -5,
        -(PanelDragController::GetDetachDockedPanelThresholdForTesting() / 2));
  }

  static gfx::Vector2d GetDragDeltaToDetach() {
    return gfx::Vector2d(
        -20,
        -(PanelDragController::GetDetachDockedPanelThresholdForTesting() + 20));
  }

  static gfx::Vector2d GetDragDeltaToRemainDetached(Panel* panel) {
    int distance =
      panel->manager()->docked_collection()->display_area().bottom() -
      panel->GetBounds().bottom();
    return gfx::Vector2d(
        -5,
        distance -
            PanelDragController::GetDockDetachedPanelThresholdForTesting() * 2);
  }

  static gfx::Vector2d GetDragDeltaToAttach(Panel* panel) {
    int distance =
        panel->manager()->docked_collection()->display_area().bottom() -
        panel->GetBounds().bottom();
    return gfx::Vector2d(
        -20,
        distance -
            PanelDragController::GetDockDetachedPanelThresholdForTesting() / 2);
  }

  // Return the delta needed to drag |panel1| to stack to the bottom of
  // |panel2|.
  static gfx::Vector2d GetDragDeltaToStackToBottom(Panel* panel1,
                                                   Panel* panel2) {
    gfx::Rect bounds1 = panel1->GetBounds();
    gfx::Rect bounds2 = panel2->GetBounds();
    return gfx::Vector2d(
        bounds2.x() - bounds1.x(),
        bounds2.bottom() - bounds1.y() +
            PanelDragController::GetGluePanelDistanceThresholdForTesting() / 2);
  }

  // Return the delta needed to drag |panel1| to unstack from the bottom of
  // |panel2|.
  static gfx::Vector2d GetDragDeltaToUnstackFromBottom(Panel* panel1,
                                                       Panel* panel2) {
    gfx::Rect bounds1 = panel1->GetBounds();
    gfx::Rect bounds2 = panel2->GetBounds();
    return gfx::Vector2d(
        0, PanelDragController::GetGluePanelDistanceThresholdForTesting() * 2);
  }

  // Return the delta needed to drag |panel1| to stack to the top of |panel2|.
  static gfx::Vector2d GetDragDeltaToStackToTop(Panel* panel1, Panel* panel2) {
    gfx::Rect bounds1 = panel1->GetBounds();
    gfx::Rect bounds2 = panel2->GetBounds();
    StackedPanelCollection* stack1 = panel1->stack();
    int bottom = stack1 ? stack1->bottom_panel()->GetBounds().bottom()
                        : bounds1.bottom();
    return gfx::Vector2d(
        bounds2.x() - bounds1.x(),
        bounds2.y() - bottom -
            PanelDragController::GetGluePanelDistanceThresholdForTesting() / 2);
  }

  // Return the delta needed to drag |panel1| to unstack from the top of
  // |panel2|.
  static gfx::Vector2d GetDragDeltaToUnstackFromTop(Panel* panel1,
                                                    Panel* panel2) {
    gfx::Rect bounds1 = panel1->GetBounds();
    gfx::Rect bounds2 = panel2->GetBounds();
    return gfx::Vector2d(
        0, -PanelDragController::GetGluePanelDistanceThresholdForTesting() * 2);
  }

  // Return the delta needed to drag |panel1| to snap to the left of |panel2|.
  static gfx::Vector2d GetDragDeltaToSnapToLeft(Panel* panel1,
                                                Panel* panel2) {
    gfx::Rect bounds1 = panel1->GetBounds();
    gfx::Rect bounds2 = panel2->GetBounds();
    return gfx::Vector2d(
        bounds2.x() - bounds1.width() - bounds1.x() -
            PanelDragController::GetGluePanelDistanceThresholdForTesting() / 2,
        bounds2.y() - bounds1.y() +
            PanelDragController::GetGluePanelDistanceThresholdForTesting() * 2);
  }

  // Return the delta needed to drag |panel1| to snap to the right of |panel2|.
  static gfx::Vector2d GetDragDeltaToSnapToRight(Panel* panel1,
                                                 Panel* panel2) {
    gfx::Rect bounds1 = panel1->GetBounds();
    gfx::Rect bounds2 = panel2->GetBounds();
    return gfx::Vector2d(
        bounds2.right() - bounds1.x() +
            PanelDragController::GetGluePanelDistanceThresholdForTesting() / 2,
        bounds2.y() - bounds1.y() +
            PanelDragController::GetGluePanelDistanceThresholdForTesting() * 2);
  }

  // Return the delta needed to drag |panel| to unsnap from its current
  // position.
  static gfx::Vector2d GetDragDeltaToUnsnap(Panel* panel) {
    return gfx::Vector2d(
        PanelDragController::GetGluePanelDistanceThresholdForTesting() * 2, 0);
  }
};

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, DragOneDockedPanel) {
  static const int big_delta_x = 70;
  static const int big_delta_y = 30;  // Do not exceed the threshold to detach.

  Panel* panel = CreateDockedPanel("1", gfx::Rect(0, 0, 100, 100));
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));
  gfx::Rect panel_old_bounds = panel->GetBounds();

  // Drag left.
  gfx::Point mouse_location = panel_old_bounds.origin();
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  mouse_location.Offset(-big_delta_x, 0);
  panel_testing->DragTitlebar(mouse_location);
  gfx::Rect panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(-big_delta_x, 0);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  panel_testing->FinishDragTitlebar();
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  // Drag left and cancel.
  mouse_location = panel_old_bounds.origin();
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  mouse_location.Offset(-big_delta_x, 0);
  panel_testing->DragTitlebar(mouse_location);
  panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(-big_delta_x, 0);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  panel_testing->CancelDragTitlebar();
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  // Drag right.
  mouse_location = panel_old_bounds.origin();
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  mouse_location.Offset(big_delta_x, 0);
  panel_testing->DragTitlebar(mouse_location);
  panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(big_delta_x, 0);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  panel_testing->FinishDragTitlebar();
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  // Drag right and up.  Expect no vertical movement.
  mouse_location = panel_old_bounds.origin();
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  mouse_location.Offset(big_delta_x, big_delta_y);
  panel_testing->DragTitlebar(mouse_location);
  panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(big_delta_x, 0);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  panel_testing->FinishDragTitlebar();
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  // Drag up.  Expect no movement on drag.
  mouse_location = panel_old_bounds.origin();
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  mouse_location.Offset(0, -big_delta_y);
  panel_testing->DragTitlebar(mouse_location);
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  panel_testing->FinishDragTitlebar();
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  // Drag down.  Expect no movement on drag.
  mouse_location = panel_old_bounds.origin();
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  mouse_location.Offset(0, big_delta_y);
  panel_testing->DragTitlebar(mouse_location);
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  panel_testing->FinishDragTitlebar();
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, DragTwoDockedPanels) {
  static const gfx::Vector2d small_delta(10, 0);

  Panel* panel1 = CreateDockedPanel("1", gfx::Rect(0, 0, 100, 100));
  Panel* panel2 = CreateDockedPanel("2", gfx::Rect(0, 0, 100, 100));
  scoped_ptr<NativePanelTesting> panel1_testing(
      CreateNativePanelTesting(panel1));
  scoped_ptr<NativePanelTesting> panel2_testing(
      CreateNativePanelTesting(panel2));
  gfx::Point position1 = panel1->GetBounds().origin();
  gfx::Point position2 = panel2->GetBounds().origin();

  // Drag right panel towards left with small delta.
  // Expect no shuffle: P1 P2
  gfx::Point mouse_location = position1;
  panel1_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(position1, panel1->GetBounds().origin());
  EXPECT_EQ(position2, panel2->GetBounds().origin());

  mouse_location = mouse_location - small_delta;
  panel1_testing->DragTitlebar(mouse_location);
  EXPECT_EQ(mouse_location, panel1->GetBounds().origin());
  EXPECT_EQ(position2, panel2->GetBounds().origin());

  panel1_testing->FinishDragTitlebar();
  EXPECT_EQ(position1, panel1->GetBounds().origin());
  EXPECT_EQ(position2, panel2->GetBounds().origin());

  // Drag right panel towards left with big delta.
  // Expect shuffle: P2 P1
  mouse_location = position1;
  panel1_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(position1, panel1->GetBounds().origin());
  EXPECT_EQ(position2, panel2->GetBounds().origin());

  mouse_location = position2 + gfx::Vector2d(1, 0);
  panel1_testing->DragTitlebar(mouse_location);
  EXPECT_EQ(mouse_location, panel1->GetBounds().origin());
  EXPECT_EQ(position1, panel2->GetBounds().origin());

  panel1_testing->FinishDragTitlebar();
  EXPECT_EQ(position2, panel1->GetBounds().origin());
  EXPECT_EQ(position1, panel2->GetBounds().origin());

  // Drag left panel towards right with small delta.
  // Expect no shuffle: P2 P1
  mouse_location = position2;
  panel1_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(position2, panel1->GetBounds().origin());
  EXPECT_EQ(position1, panel2->GetBounds().origin());

  mouse_location = mouse_location + small_delta;
  panel1_testing->DragTitlebar(mouse_location);
  EXPECT_EQ(mouse_location, panel1->GetBounds().origin());
  EXPECT_EQ(position1, panel2->GetBounds().origin());

  panel1_testing->FinishDragTitlebar();
  EXPECT_EQ(position2, panel1->GetBounds().origin());
  EXPECT_EQ(position1, panel2->GetBounds().origin());

  // Drag left panel towards right with big delta.
  // Expect shuffle: P1 P2
  mouse_location = position2;
  panel1_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(position2, panel1->GetBounds().origin());
  EXPECT_EQ(position1, panel2->GetBounds().origin());

  mouse_location = position1 + gfx::Vector2d(1, 0);
  panel1_testing->DragTitlebar(mouse_location);
  EXPECT_EQ(mouse_location, panel1->GetBounds().origin());
  EXPECT_EQ(position2, panel2->GetBounds().origin());

  panel1_testing->FinishDragTitlebar();
  EXPECT_EQ(position1, panel1->GetBounds().origin());
  EXPECT_EQ(position2, panel2->GetBounds().origin());

  // Drag right panel towards left with big delta and then cancel the drag.
  // Expect shuffle after drag:   P2 P1
  // Expect shuffle after cancel: P1 P2
  mouse_location = position1;
  panel1_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(position1, panel1->GetBounds().origin());
  EXPECT_EQ(position2, panel2->GetBounds().origin());

  mouse_location = position2 + gfx::Vector2d(1, 0);
  panel1_testing->DragTitlebar(mouse_location);
  EXPECT_EQ(mouse_location, panel1->GetBounds().origin());
  EXPECT_EQ(position1, panel2->GetBounds().origin());

  panel1_testing->CancelDragTitlebar();
  EXPECT_EQ(position1, panel1->GetBounds().origin());
  EXPECT_EQ(position2, panel2->GetBounds().origin());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, DragThreeDockedPanels) {
  Panel* panel1 = CreateDockedPanel("1", gfx::Rect(0, 0, 100, 100));
  Panel* panel2 = CreateDockedPanel("2", gfx::Rect(0, 0, 100, 100));
  Panel* panel3 = CreateDockedPanel("3", gfx::Rect(0, 0, 100, 100));
  scoped_ptr<NativePanelTesting> panel2_testing(
      CreateNativePanelTesting(panel2));
  scoped_ptr<NativePanelTesting> panel3_testing(
      CreateNativePanelTesting(panel3));
  gfx::Point position1 = panel1->GetBounds().origin();
  gfx::Point position2 = panel2->GetBounds().origin();
  gfx::Point position3 = panel3->GetBounds().origin();

  // Drag leftmost panel to become the rightmost in 2 drags. Each drag will
  // shuffle one panel.
  // Expect shuffle after 1st drag: P1 P3 P2
  // Expect shuffle after 2nd drag: P3 P1 P2
  gfx::Point mouse_location = position3;
  panel3_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(position1, panel1->GetBounds().origin());
  EXPECT_EQ(position2, panel2->GetBounds().origin());
  EXPECT_EQ(position3, panel3->GetBounds().origin());

  mouse_location = position2 + gfx::Vector2d(1, 0);
  panel3_testing->DragTitlebar(mouse_location);
  EXPECT_EQ(position1, panel1->GetBounds().origin());
  EXPECT_EQ(position3, panel2->GetBounds().origin());
  EXPECT_EQ(mouse_location, panel3->GetBounds().origin());

  mouse_location = position1 + gfx::Vector2d(1, 0);
  panel3_testing->DragTitlebar(mouse_location);
  EXPECT_EQ(position2, panel1->GetBounds().origin());
  EXPECT_EQ(position3, panel2->GetBounds().origin());
  EXPECT_EQ(mouse_location, panel3->GetBounds().origin());

  panel3_testing->FinishDragTitlebar();
  EXPECT_EQ(position2, panel1->GetBounds().origin());
  EXPECT_EQ(position3, panel2->GetBounds().origin());
  EXPECT_EQ(position1, panel3->GetBounds().origin());

  // Drag rightmost panel to become the leftmost in 2 drags and then cancel the
  // drag. Each drag will shuffle one panel and the cancellation will restore
  // all panels.
  // Expect shuffle after 1st drag: P1 P3 P2
  // Expect shuffle after 2nd drag: P1 P2 P3
  // Expect shuffle after cancel:   P3 P1 P2
  mouse_location = position1;
  panel3_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(position2, panel1->GetBounds().origin());
  EXPECT_EQ(position3, panel2->GetBounds().origin());
  EXPECT_EQ(position1, panel3->GetBounds().origin());

  mouse_location = position2 + gfx::Vector2d(1, 0);
  panel3_testing->DragTitlebar(mouse_location);
  EXPECT_EQ(position1, panel1->GetBounds().origin());
  EXPECT_EQ(position3, panel2->GetBounds().origin());
  EXPECT_EQ(mouse_location, panel3->GetBounds().origin());

  mouse_location = position3 + gfx::Vector2d(1, 0);
  panel3_testing->DragTitlebar(mouse_location);
  EXPECT_EQ(position1, panel1->GetBounds().origin());
  EXPECT_EQ(position2, panel2->GetBounds().origin());
  EXPECT_EQ(mouse_location, panel3->GetBounds().origin());

  panel3_testing->CancelDragTitlebar();
  EXPECT_EQ(position2, panel1->GetBounds().origin());
  EXPECT_EQ(position3, panel2->GetBounds().origin());
  EXPECT_EQ(position1, panel3->GetBounds().origin());

  // Drag leftmost panel to become the rightmost in a single drag. The drag will
  // shuffle 2 panels at a time.
  // Expect shuffle: P2 P3 P1
  mouse_location = position3;
  panel2_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(position2, panel1->GetBounds().origin());
  EXPECT_EQ(position3, panel2->GetBounds().origin());
  EXPECT_EQ(position1, panel3->GetBounds().origin());

  mouse_location = position1 + gfx::Vector2d(1, 0);
  panel2_testing->DragTitlebar(mouse_location);
  EXPECT_EQ(position3, panel1->GetBounds().origin());
  EXPECT_EQ(mouse_location, panel2->GetBounds().origin());
  EXPECT_EQ(position2, panel3->GetBounds().origin());

  panel2_testing->FinishDragTitlebar();
  EXPECT_EQ(position3, panel1->GetBounds().origin());
  EXPECT_EQ(position1, panel2->GetBounds().origin());
  EXPECT_EQ(position2, panel3->GetBounds().origin());

  // Drag rightmost panel to become the leftmost in a single drag. The drag will
  // shuffle 2 panels at a time.
  // Expect shuffle: P3 P1 P2
  mouse_location = position1;
  panel2_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(position3, panel1->GetBounds().origin());
  EXPECT_EQ(position1, panel2->GetBounds().origin());
  EXPECT_EQ(position2, panel3->GetBounds().origin());

  mouse_location = position3 + gfx::Vector2d(1, 0);
  panel2_testing->DragTitlebar(mouse_location);
  EXPECT_EQ(position2, panel1->GetBounds().origin());
  EXPECT_EQ(mouse_location, panel2->GetBounds().origin());
  EXPECT_EQ(position1, panel3->GetBounds().origin());

  panel2_testing->FinishDragTitlebar();
  EXPECT_EQ(position2, panel1->GetBounds().origin());
  EXPECT_EQ(position3, panel2->GetBounds().origin());
  EXPECT_EQ(position1, panel3->GetBounds().origin());

  // Drag rightmost panel to become the leftmost in a single drag and then
  // cancel the drag. The drag will shuffle 2 panels and the cancellation will
  // restore all panels.
  // Expect shuffle after drag:   P1 P2 P3
  // Expect shuffle after cancel: P3 P1 P2
  mouse_location = position1;
  panel3_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(position2, panel1->GetBounds().origin());
  EXPECT_EQ(position3, panel2->GetBounds().origin());
  EXPECT_EQ(position1, panel3->GetBounds().origin());

  mouse_location = position3 + gfx::Vector2d(1, 0);
  panel3_testing->DragTitlebar(mouse_location);
  EXPECT_EQ(position1, panel1->GetBounds().origin());
  EXPECT_EQ(position2, panel2->GetBounds().origin());
  EXPECT_EQ(mouse_location, panel3->GetBounds().origin());

  panel3_testing->CancelDragTitlebar();
  EXPECT_EQ(position2, panel1->GetBounds().origin());
  EXPECT_EQ(position3, panel2->GetBounds().origin());
  EXPECT_EQ(position1, panel3->GetBounds().origin());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, DragMinimizedPanel) {
  Panel* panel = CreatePanel("panel1");
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));

  panel->Minimize();
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());

  // Hover over minimized panel to bring up titlebar.
  gfx::Point hover_point(panel->GetBounds().origin());
  MoveMouseAndWaitForExpansionStateChange(panel, hover_point);
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());

  // Verify we can drag a minimized panel.
  gfx::Rect panel_old_bounds = panel->GetBounds();
  gfx::Point mouse_location = panel_old_bounds.origin();
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());

  mouse_location.Offset(-70, 0);
  panel_testing->DragTitlebar(mouse_location);
  gfx::Rect panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(-70, 0);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());

  // Verify panel returns to fully minimized state after dragging ends once
  // mouse moves away from panel.
  panel_testing->FinishDragTitlebar();
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());

  MoveMouseAndWaitForExpansionStateChange(panel, mouse_location);
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest,
                       DragMinimizedPanelWhileDrawingAttention) {
  Panel* panel = CreatePanel("panel1");
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));
  CreatePanel("panel2");

  panel->Minimize();
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());

  panel->FlashFrame(true);
  EXPECT_TRUE(panel->IsDrawingAttention());
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());

  // Drag the panel. Verify panel stays in title-only state after attention is
  // cleared because it is being dragged.
  gfx::Rect panel_old_bounds = panel->GetBounds();
  gfx::Point mouse_location = panel_old_bounds.origin();
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());

  mouse_location.Offset(-70, 0);
  panel_testing->DragTitlebar(mouse_location);
  gfx::Rect panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(-70, 0);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  panel->FlashFrame(false);
  EXPECT_FALSE(panel->IsDrawingAttention());
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());

  // Typical user scenario will detect the mouse in the panel
  // after attention is cleared, causing titles to pop up, so
  // we simulate that here.
  MoveMouse(mouse_location);

  // Verify panel returns to fully minimized state after dragging ends once
  // mouse moves away from the panel.
  panel_testing->FinishDragTitlebar();
  EXPECT_EQ(Panel::TITLE_ONLY, panel->expansion_state());

  mouse_location.Offset(0, -50);
  MoveMouseAndWaitForExpansionStateChange(panel, mouse_location);
  EXPECT_EQ(Panel::MINIMIZED, panel->expansion_state());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, CloseDockedPanelOnDrag) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  PanelDragController* drag_controller = panel_manager->drag_controller();
  DockedPanelCollection* docked_collection = panel_manager->docked_collection();

  // Create 4 docked panels.
  // We have:  P4  P3  P2  P1
  Panel* panel1 = CreatePanelWithBounds("Panel1", gfx::Rect(0, 0, 100, 100));
  Panel* panel2 = CreatePanelWithBounds("Panel2", gfx::Rect(0, 0, 100, 100));
  Panel* panel3 = CreatePanelWithBounds("Panel3", gfx::Rect(0, 0, 100, 100));
  Panel* panel4 = CreatePanelWithBounds("Panel4", gfx::Rect(0, 0, 100, 100));
  ASSERT_EQ(4, docked_collection->num_panels());

  scoped_ptr<NativePanelTesting> panel1_testing(
      CreateNativePanelTesting(panel1));
  gfx::Point position1 = panel1->GetBounds().origin();
  gfx::Point position2 = panel2->GetBounds().origin();
  gfx::Point position3 = panel3->GetBounds().origin();
  gfx::Point position4 = panel4->GetBounds().origin();

  // Test the scenario: drag a panel, close another panel, cancel the drag.
  {
    std::vector<Panel*> panels;
    gfx::Point panel1_new_position = position1;
    panel1_new_position.Offset(-500, 0);

    // Start dragging a panel.
    // We have:  P1*  P4  P3  P2
    gfx::Point mouse_location = panel1->GetBounds().origin();
    panel1_testing->PressLeftMouseButtonTitlebar(mouse_location);
    mouse_location.Offset(-500, -5);
    panel1_testing->DragTitlebar(mouse_location);
    EXPECT_TRUE(drag_controller->is_dragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());

    ASSERT_EQ(4, docked_collection->num_panels());
    panels = PanelManager::GetInstance()->panels();
    EXPECT_EQ(panel2, panels[0]);
    EXPECT_EQ(panel3, panels[1]);
    EXPECT_EQ(panel4, panels[2]);
    EXPECT_EQ(panel1, panels[3]);
    EXPECT_EQ(position1, panel2->GetBounds().origin());
    EXPECT_EQ(position2, panel3->GetBounds().origin());
    EXPECT_EQ(position3, panel4->GetBounds().origin());
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());

    // Closing another panel while dragging in progress will keep the dragging
    // panel intact.
    // We have:  P1*  P4  P3
    CloseWindowAndWait(panel2);
    EXPECT_TRUE(drag_controller->is_dragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());

    ASSERT_EQ(3, docked_collection->num_panels());
    panels = PanelManager::GetInstance()->panels();
    EXPECT_EQ(panel3, panels[0]);
    EXPECT_EQ(panel4, panels[1]);
    EXPECT_EQ(panel1, panels[2]);
    EXPECT_EQ(position1, panel3->GetBounds().origin());
    EXPECT_EQ(position2, panel4->GetBounds().origin());
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());

    // Cancel the drag.
    // We have:  P4  P3  P1
    panel1_testing->CancelDragTitlebar();
    EXPECT_FALSE(drag_controller->is_dragging());

    ASSERT_EQ(3, docked_collection->num_panels());
    panels = PanelManager::GetInstance()->panels();
    EXPECT_EQ(panel1, panels[0]);
    EXPECT_EQ(panel3, panels[1]);
    EXPECT_EQ(panel4, panels[2]);
    EXPECT_EQ(position1, panel1->GetBounds().origin());
    EXPECT_EQ(position2, panel3->GetBounds().origin());
    EXPECT_EQ(position3, panel4->GetBounds().origin());
  }

  // Test the scenario: drag a panel, close another panel, end the drag.
  {
    std::vector<Panel*> panels;
    gfx::Point panel1_new_position = position1;
    panel1_new_position.Offset(-500, 0);

    // Start dragging a panel.
    // We have:  P1*  P4  P3
    gfx::Point mouse_location = panel1->GetBounds().origin();
    panel1_testing->PressLeftMouseButtonTitlebar(mouse_location);
    mouse_location.Offset(-500, -5);
    panel1_testing->DragTitlebar(mouse_location);
    EXPECT_TRUE(drag_controller->is_dragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());

    ASSERT_EQ(3, docked_collection->num_panels());
    panels = PanelManager::GetInstance()->panels();
    EXPECT_EQ(panel3, panels[0]);
    EXPECT_EQ(panel4, panels[1]);
    EXPECT_EQ(panel1, panels[2]);
    EXPECT_EQ(position1, panel3->GetBounds().origin());
    EXPECT_EQ(position2, panel4->GetBounds().origin());
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());

    // Closing another panel while dragging in progress will keep the dragging
    // panel intact.
    // We have:  P1*  P4
    CloseWindowAndWait(panel3);
    EXPECT_TRUE(drag_controller->is_dragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());

    ASSERT_EQ(2, docked_collection->num_panels());
    panels = PanelManager::GetInstance()->panels();
    EXPECT_EQ(panel4, panels[0]);
    EXPECT_EQ(panel1, panels[1]);
    EXPECT_EQ(position1, panel4->GetBounds().origin());
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());

    // Finish the drag.
    // We have:  P1  P4
    panel1_testing->FinishDragTitlebar();
    EXPECT_FALSE(drag_controller->is_dragging());

    ASSERT_EQ(2, docked_collection->num_panels());
    panels = PanelManager::GetInstance()->panels();
    EXPECT_EQ(panel4, panels[0]);
    EXPECT_EQ(panel1, panels[1]);
    EXPECT_EQ(position1, panel4->GetBounds().origin());
    EXPECT_EQ(position2, panel1->GetBounds().origin());
  }

  // Test the scenario: drag a panel and close the dragging panel.
  {
    std::vector<Panel*> panels;
    gfx::Point panel1_new_position = position2;
    panel1_new_position.Offset(-500, 0);

    // Start dragging a panel again.
    // We have:  P1*  P4
    gfx::Point mouse_location = panel1->GetBounds().origin();
    panel1_testing->PressLeftMouseButtonTitlebar(mouse_location);
    mouse_location.Offset(-500, -5);
    panel1_testing->DragTitlebar(mouse_location);
    EXPECT_TRUE(drag_controller->is_dragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());

    ASSERT_EQ(2, docked_collection->num_panels());
    panels = PanelManager::GetInstance()->panels();
    EXPECT_EQ(panel4, panels[0]);
    EXPECT_EQ(panel1, panels[1]);
    EXPECT_EQ(position1, panel4->GetBounds().origin());

    // Closing the dragging panel should make the drag controller abort.
    // We have:  P4
    content::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_PANEL_CLOSED, content::Source<Panel>(panel1));
    panel1->Close();
    EXPECT_FALSE(drag_controller->is_dragging());

    // Continue the drag to ensure the drag controller does not crash.
    panel1_new_position.Offset(20, 30);
    panel1_testing->DragTitlebar(panel1_new_position);
    panel1_testing->FinishDragTitlebar();

    // Wait till the panel is fully closed.
    signal.Wait();
    ASSERT_EQ(1, docked_collection->num_panels());
    panels = PanelManager::GetInstance()->panels();
    EXPECT_EQ(panel4, panels[0]);
    EXPECT_EQ(position1, panel4->GetBounds().origin());
  }

  panel_manager->CloseAll();
}

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_DragOneDetachedPanel DISABLED_DragOneDetachedPanel
#else
#define MAYBE_DragOneDetachedPanel DragOneDetachedPanel
#endif
IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, MAYBE_DragOneDetachedPanel) {
  Panel* panel = CreateDetachedPanel("1", gfx::Rect(300, 200, 250, 200));

  // Test that the detached panel can be dragged almost anywhere except getting
  // close to the bottom of the docked area to trigger the attach.
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));
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

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest,
                       DISABLED_CloseDetachedPanelOnDrag) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  PanelDragController* drag_controller = panel_manager->drag_controller();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 4 detached panels.
  Panel* panel1 = CreateDetachedPanel("1", gfx::Rect(100, 200, 100, 100));
  Panel* panel2 = CreateDetachedPanel("2", gfx::Rect(200, 210, 110, 110));
  Panel* panel3 = CreateDetachedPanel("3", gfx::Rect(300, 220, 120, 120));
  Panel* panel4 = CreateDetachedPanel("4", gfx::Rect(400, 230, 130, 130));
  ASSERT_EQ(4, detached_collection->num_panels());

  scoped_ptr<NativePanelTesting> panel1_testing(
      CreateNativePanelTesting(panel1));
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
    EXPECT_TRUE(drag_controller->is_dragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());

    ASSERT_EQ(4, detached_collection->num_panels());
    EXPECT_TRUE(detached_collection->HasPanel(panel1));
    EXPECT_TRUE(detached_collection->HasPanel(panel2));
    EXPECT_TRUE(detached_collection->HasPanel(panel3));
    EXPECT_TRUE(detached_collection->HasPanel(panel4));
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());
    EXPECT_EQ(panel2_position, panel2->GetBounds().origin());
    EXPECT_EQ(panel3_position, panel3->GetBounds().origin());
    EXPECT_EQ(panel4_position, panel4->GetBounds().origin());

    // Closing another panel while dragging in progress will keep the dragging
    // panel intact.
    CloseWindowAndWait(panel2);
    EXPECT_TRUE(drag_controller->is_dragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());

    ASSERT_EQ(3, detached_collection->num_panels());
    EXPECT_TRUE(detached_collection->HasPanel(panel1));
    EXPECT_TRUE(detached_collection->HasPanel(panel3));
    EXPECT_TRUE(detached_collection->HasPanel(panel4));
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());
    EXPECT_EQ(panel3_position, panel3->GetBounds().origin());
    EXPECT_EQ(panel4_position, panel4->GetBounds().origin());

    // Cancel the drag.
    panel1_testing->CancelDragTitlebar();
    WaitForBoundsAnimationFinished(panel1);
    EXPECT_FALSE(drag_controller->is_dragging());

    ASSERT_EQ(3, detached_collection->num_panels());
    EXPECT_TRUE(detached_collection->HasPanel(panel1));
    EXPECT_TRUE(detached_collection->HasPanel(panel3));
    EXPECT_TRUE(detached_collection->HasPanel(panel4));
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
    EXPECT_TRUE(drag_controller->is_dragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());

    ASSERT_EQ(3, detached_collection->num_panels());
    EXPECT_TRUE(detached_collection->HasPanel(panel1));
    EXPECT_TRUE(detached_collection->HasPanel(panel3));
    EXPECT_TRUE(detached_collection->HasPanel(panel4));
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());
    EXPECT_EQ(panel3_position, panel3->GetBounds().origin());
    EXPECT_EQ(panel4_position, panel4->GetBounds().origin());

    // Closing another panel while dragging in progress will keep the dragging
    // panel intact.
    CloseWindowAndWait(panel3);
    EXPECT_TRUE(drag_controller->is_dragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());

    ASSERT_EQ(2, detached_collection->num_panels());
    EXPECT_TRUE(detached_collection->HasPanel(panel1));
    EXPECT_TRUE(detached_collection->HasPanel(panel4));
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());
    EXPECT_EQ(panel4_position, panel4->GetBounds().origin());

    // Finish the drag.
    panel1_testing->FinishDragTitlebar();
    EXPECT_FALSE(drag_controller->is_dragging());

    ASSERT_EQ(2, detached_collection->num_panels());
    EXPECT_TRUE(detached_collection->HasPanel(panel1));
    EXPECT_TRUE(detached_collection->HasPanel(panel4));
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
    EXPECT_TRUE(drag_controller->is_dragging());
    EXPECT_EQ(panel1, drag_controller->dragging_panel());

    ASSERT_EQ(2, detached_collection->num_panels());
    EXPECT_TRUE(detached_collection->HasPanel(panel1));
    EXPECT_TRUE(detached_collection->HasPanel(panel4));
    EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());
    EXPECT_EQ(panel4_position, panel4->GetBounds().origin());

    // Closing the dragging panel should make the drag controller abort.
    content::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_PANEL_CLOSED, content::Source<Panel>(panel1));
    panel1->Close();
    EXPECT_FALSE(drag_controller->is_dragging());

    // Continue the drag to ensure the drag controller does not crash.
    panel1_new_position.Offset(20, 30);
    panel1_testing->DragTitlebar(panel1_new_position);
    panel1_testing->FinishDragTitlebar();

    // Wait till the panel is fully closed.
    signal.Wait();
    ASSERT_EQ(1, detached_collection->num_panels());
    EXPECT_TRUE(detached_collection->HasPanel(panel4));
    EXPECT_EQ(panel4_position, panel4->GetBounds().origin());
  }

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, Detach) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelCollection* docked_collection = panel_manager->docked_collection();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create one docked panel.
  Panel* panel = CreateDockedPanel("1", gfx::Rect(0, 0, 100, 100));
  ASSERT_EQ(1, docked_collection->num_panels());
  ASSERT_EQ(0, detached_collection->num_panels());

  gfx::Rect panel_old_bounds = panel->GetBounds();

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));
  gfx::Point mouse_location(panel->GetBounds().origin());
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag up the panel in a small offset that does not trigger the detach.
  // Expect that the panel is still docked and only x coordinate of its position
  // is changed.
  gfx::Vector2d drag_delta_to_remain_docked = GetDragDeltaToRemainDocked();
  mouse_location = mouse_location + drag_delta_to_remain_docked;
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(1, docked_collection->num_panels());
  ASSERT_EQ(0, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DOCKED, panel->collection()->type());
  gfx::Rect panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(drag_delta_to_remain_docked.x(), 0);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Continue dragging up the panel in big offset that triggers the detach.
  // Expect that the panel is previewed as detached.
  gfx::Vector2d drag_delta_to_detach = GetDragDeltaToDetach();
  mouse_location = mouse_location + drag_delta_to_detach;
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, docked_collection->num_panels());
  ASSERT_EQ(1, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel->collection()->type());
  panel_new_bounds.Offset(
      drag_delta_to_detach.x(),
      drag_delta_to_detach.y() + drag_delta_to_remain_docked.y());
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Finish the drag.
  // Expect that the panel stays as detached.
  panel_testing->FinishDragTitlebar();
  ASSERT_EQ(0, docked_collection->num_panels());
  ASSERT_EQ(1, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel->collection()->type());
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  panel_manager->CloseAll();
}

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_DetachAndCancel DISABLED_DetachAndCancel
#else
#define MAYBE_DetachAndCancel DetachAndCancel
#endif
IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, MAYBE_DetachAndCancel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelCollection* docked_collection = panel_manager->docked_collection();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create one docked panel.
  Panel* panel = CreateDockedPanel("1", gfx::Rect(0, 0, 100, 100));
  ASSERT_EQ(1, docked_collection->num_panels());
  ASSERT_EQ(0, detached_collection->num_panels());

  gfx::Rect panel_old_bounds = panel->GetBounds();

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));
  gfx::Point mouse_location(panel->GetBounds().origin());
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag up the panel in a small offset that does not trigger the detach.
  // Expect that the panel is still docked and only x coordinate of its position
  // is changed.
  gfx::Vector2d drag_delta_to_remain_docked = GetDragDeltaToRemainDocked();
  mouse_location = mouse_location + drag_delta_to_remain_docked;
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(1, docked_collection->num_panels());
  ASSERT_EQ(0, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DOCKED, panel->collection()->type());
  gfx::Rect panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(drag_delta_to_remain_docked.x(), 0);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Continue dragging up the panel in big offset that triggers the detach.
  // Expect that the panel is previewed as detached.
  gfx::Vector2d drag_delta_to_detach = GetDragDeltaToDetach();
  mouse_location = mouse_location + drag_delta_to_detach;
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, docked_collection->num_panels());
  ASSERT_EQ(1, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel->collection()->type());
  panel_new_bounds.Offset(
      drag_delta_to_detach.x(),
      drag_delta_to_detach.y() + drag_delta_to_remain_docked.y());
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Cancel the drag.
  // Expect that the panel is back as docked.
  panel_testing->CancelDragTitlebar();
  ASSERT_EQ(1, docked_collection->num_panels());
  ASSERT_EQ(0, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DOCKED, panel->collection()->type());
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  panel_manager->CloseAll();
}

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_Attach DISABLED_Attach
#else
#define MAYBE_Attach Attach
#endif
IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, MAYBE_Attach) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelCollection* docked_collection = panel_manager->docked_collection();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create one detached panel.
  Panel* panel = CreateDetachedPanel("1", gfx::Rect(400, 300, 100, 100));
  ASSERT_EQ(0, docked_collection->num_panels());
  ASSERT_EQ(1, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel->collection()->type());

  gfx::Rect panel_old_bounds = panel->GetBounds();

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));
  gfx::Point mouse_location(panel->GetBounds().origin());
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag down the panel but not close enough to the bottom of work area.
  // Expect that the panel is still detached.
  gfx::Vector2d drag_delta_to_remain_detached =
      GetDragDeltaToRemainDetached(panel);
  mouse_location = mouse_location + drag_delta_to_remain_detached;
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, docked_collection->num_panels());
  ASSERT_EQ(1, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel->collection()->type());
  gfx::Rect panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(drag_delta_to_remain_detached);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Continue dragging down the panel to make it close enough to the bottom of
  // work area.
  // Expect that the panel is previewed as docked.
  gfx::Vector2d drag_delta_to_attach = GetDragDeltaToAttach(panel);
  mouse_location = mouse_location + drag_delta_to_attach;
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(1, docked_collection->num_panels());
  ASSERT_EQ(0, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DOCKED, panel->collection()->type());
  panel_new_bounds.Offset(drag_delta_to_attach);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Finish the drag.
  // Expect that the panel stays as docked and moves to the final position.
  panel_testing->FinishDragTitlebar();
  ASSERT_EQ(1, docked_collection->num_panels());
  ASSERT_EQ(0, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DOCKED, panel->collection()->type());
  panel_new_bounds.set_x(
      docked_collection->StartingRightPosition() - panel_new_bounds.width());
  panel_new_bounds.set_y(
      docked_collection->display_area().bottom() - panel_new_bounds.height());
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  panel_manager->CloseAll();
}

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_AttachAndCancel DISABLED_AttachAndCancel
#else
#define MAYBE_AttachAndCancel AttachAndCancel
#endif
IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, MAYBE_AttachAndCancel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelCollection* docked_collection = panel_manager->docked_collection();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create one detached panel.
  Panel* panel = CreateDetachedPanel("1", gfx::Rect(400, 300, 100, 100));
  ASSERT_EQ(0, docked_collection->num_panels());
  ASSERT_EQ(1, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel->collection()->type());

  gfx::Rect panel_old_bounds = panel->GetBounds();

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));
  gfx::Point mouse_location(panel->GetBounds().origin());
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag down the panel but not close enough to the bottom of work area.
  // Expect that the panel is still detached.
  gfx::Vector2d drag_delta_to_remain_detached =
      GetDragDeltaToRemainDetached(panel);
  mouse_location = mouse_location + drag_delta_to_remain_detached;
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, docked_collection->num_panels());
  ASSERT_EQ(1, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel->collection()->type());
  gfx::Rect panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(drag_delta_to_remain_detached);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Continue dragging down the panel to make it close enough to the bottom of
  // work area.
  // Expect that the panel is previewed as docked.
  gfx::Vector2d drag_delta_to_attach = GetDragDeltaToAttach(panel);
  mouse_location = mouse_location + drag_delta_to_attach;
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(1, docked_collection->num_panels());
  ASSERT_EQ(0, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DOCKED, panel->collection()->type());
  panel_new_bounds.Offset(drag_delta_to_attach);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Cancel the drag.
  // Expect that the panel is back as detached.
  panel_testing->CancelDragTitlebar();
  ASSERT_EQ(0, docked_collection->num_panels());
  ASSERT_EQ(1, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel->collection()->type());
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, DetachAttachAndCancel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelCollection* docked_collection = panel_manager->docked_collection();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create one docked panel.
  Panel* panel = CreateDockedPanel("1", gfx::Rect(0, 0, 100, 100));
  ASSERT_EQ(1, docked_collection->num_panels());
  ASSERT_EQ(0, detached_collection->num_panels());

  gfx::Rect panel_old_bounds = panel->GetBounds();

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel_testing(
      CreateNativePanelTesting(panel));
  gfx::Point mouse_location(panel->GetBounds().origin());
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag up the panel to trigger the detach.
  // Expect that the panel is previewed as detached.
  gfx::Vector2d drag_delta_to_detach = GetDragDeltaToDetach();
  mouse_location = mouse_location + drag_delta_to_detach;
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, docked_collection->num_panels());
  ASSERT_EQ(1, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel->collection()->type());
  gfx::Rect panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(drag_delta_to_detach);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Continue dragging down the panel to trigger the re-attach.
  gfx::Vector2d drag_delta_to_reattach = GetDragDeltaToAttach(panel);
  mouse_location = mouse_location + drag_delta_to_reattach;
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(1, docked_collection->num_panels());
  ASSERT_EQ(0, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DOCKED, panel->collection()->type());
  panel_new_bounds.Offset(drag_delta_to_reattach);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Continue dragging up the panel to trigger the detach again.
  gfx::Vector2d drag_delta_to_detach_again = GetDragDeltaToDetach();
  mouse_location = mouse_location + drag_delta_to_detach_again;
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, docked_collection->num_panels());
  ASSERT_EQ(1, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel->collection()->type());
  panel_new_bounds.Offset(drag_delta_to_detach_again);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Cancel the drag.
  // Expect that the panel stays as docked.
  panel_testing->CancelDragTitlebar();
  ASSERT_EQ(1, docked_collection->num_panels());
  ASSERT_EQ(0, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DOCKED, panel->collection()->type());
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  panel_manager->CloseAll();
}

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_DetachWithSqueeze DISABLED_DetachWithSqueeze
#else
#define MAYBE_DetachWithSqueeze DetachWithSqueeze
#endif
IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, MAYBE_DetachWithSqueeze) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelCollection* docked_collection = panel_manager->docked_collection();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  gfx::Vector2d drag_delta_to_detach = GetDragDeltaToDetach();

  // Create some docked panels.
  //   docked:    P1  P2  P3  P4  P5
  Panel* panel1 = CreateDockedPanel("1", gfx::Rect(0, 0, 200, 100));
  Panel* panel2 = CreateDockedPanel("2", gfx::Rect(0, 0, 200, 100));
  Panel* panel3 = CreateDockedPanel("3", gfx::Rect(0, 0, 200, 100));
  Panel* panel4 = CreateDockedPanel("4", gfx::Rect(0, 0, 200, 100));
  Panel* panel5 = CreateDockedPanel("5", gfx::Rect(0, 0, 200, 100));
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(5, docked_collection->num_panels());

  // Drag to detach the middle docked panel.
  // Expect to have:
  //   detached:  P2
  //   docked:    P1  P3  P4 P5
  gfx::Point panel2_docked_position = panel2->GetBounds().origin();
  DragPanelByDelta(panel2, drag_delta_to_detach);
  ASSERT_EQ(1, detached_collection->num_panels());
  ASSERT_EQ(4, docked_collection->num_panels());
  EXPECT_EQ(PanelCollection::DOCKED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel3->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel4->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel5->collection()->type());
  gfx::Point panel2_new_position =
      panel2_docked_position + drag_delta_to_detach;
  EXPECT_EQ(panel2_new_position, panel2->GetBounds().origin());

  // Drag to detach the left-most docked panel.
  // Expect to have:
  //   detached:  P2  P4
  //   docked:    P1  P3  P5
  gfx::Point panel4_docked_position = panel4->GetBounds().origin();
  DragPanelByDelta(panel4, drag_delta_to_detach);
  ASSERT_EQ(2, detached_collection->num_panels());
  ASSERT_EQ(3, docked_collection->num_panels());
  EXPECT_EQ(PanelCollection::DOCKED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel3->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel4->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel5->collection()->type());
  EXPECT_EQ(panel2_new_position, panel2->GetBounds().origin());
  gfx::Point panel4_new_position =
      panel4_docked_position + drag_delta_to_detach;
  EXPECT_EQ(panel4_new_position, panel4->GetBounds().origin());

  // Drag to detach the right-most docked panel.
  // Expect to have:
  //   detached:  P1  P2  P4
  //   docked:    P3  P5
  gfx::Point docked_position1 = panel1->GetBounds().origin();
  gfx::Point docked_position2 = panel3->GetBounds().origin();

  DragPanelByDelta(panel1, drag_delta_to_detach);
  ASSERT_EQ(3, detached_collection->num_panels());
  ASSERT_EQ(2, docked_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel3->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel4->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel5->collection()->type());
  gfx::Point panel1_new_position = docked_position1 + drag_delta_to_detach;
  EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());
  EXPECT_EQ(panel2_new_position, panel2->GetBounds().origin());
  EXPECT_EQ(panel4_new_position, panel4->GetBounds().origin());

  // No more squeeze, docked panels should stay put.
  EXPECT_EQ(docked_position1, panel3->GetBounds().origin());
  EXPECT_EQ(panel1->GetBounds().width(), panel1->GetRestoredBounds().width());
  EXPECT_EQ(docked_position2, panel5->GetBounds().origin());
  EXPECT_EQ(panel2->GetBounds().width(), panel2->GetRestoredBounds().width());

  panel_manager->CloseAll();
}

// http://crbug.com/143247, http://crbug.com/175760
#if defined(OS_LINUX) || defined(OS_MACOSX)
#define MAYBE_AttachWithSqueeze DISABLED_AttachWithSqueeze
#else
#define MAYBE_AttachWithSqueeze AttachWithSqueeze
#endif
IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, MAYBE_AttachWithSqueeze) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelCollection* docked_collection = panel_manager->docked_collection();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create some detached, docked panels.
  //   detached:  P1  P2  P3
  //   docked:    P4  P5  P6  P7
  Panel* panel1 = CreateDetachedPanel("1", gfx::Rect(100, 300, 200, 100));
  Panel* panel2 = CreateDetachedPanel("2", gfx::Rect(200, 300, 200, 100));
  Panel* panel3 = CreateDetachedPanel("3", gfx::Rect(400, 300, 200, 100));
  Panel* panel4 = CreateDockedPanel("4", gfx::Rect(0, 0, 200, 100));
  Panel* panel5 = CreateDockedPanel("5", gfx::Rect(0, 0, 200, 100));
  Panel* panel6 = CreateDockedPanel("6", gfx::Rect(0, 0, 200, 100));
  Panel* panel7 = CreateDockedPanel("7", gfx::Rect(0, 0, 200, 100));
  ASSERT_EQ(3, detached_collection->num_panels());
  ASSERT_EQ(4, docked_collection->num_panels());

  // Wait for active states to settle.
  PanelCollectionSqueezeObserver panel7_settled(docked_collection, panel7);
  panel7_settled.Wait();

  gfx::Point detached_position1 = panel1->GetBounds().origin();
  gfx::Point detached_position2 = panel2->GetBounds().origin();
  gfx::Point detached_position3 = panel3->GetBounds().origin();
  gfx::Point docked_position4 = panel4->GetBounds().origin();
  gfx::Point docked_position5 = panel5->GetBounds().origin();
  gfx::Point docked_position6 = panel6->GetBounds().origin();
  gfx::Point docked_position7 = panel7->GetBounds().origin();

  // Drag to attach a detached panel between 2 docked panels.
  // Expect to have:
  //   detached:  P1  P2
  //   docked:    P4  P3  P5  P6  P7
  gfx::Point drag_to_location(panel5->GetBounds().x() + 10,
                              panel5->GetBounds().y());
  DragPanelToMouseLocation(panel3, drag_to_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  ASSERT_EQ(5, docked_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel3->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel4->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel5->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel6->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel7->collection()->type());
  EXPECT_EQ(detached_position1, panel1->GetBounds().origin());
  EXPECT_EQ(detached_position2, panel2->GetBounds().origin());

  // Wait for active states to settle.
  MessageLoopForUI::current()->RunUntilIdle();

  // Panel positions should have shifted because of the "squeeze" mode.
  EXPECT_NE(docked_position4, panel4->GetBounds().origin());
  EXPECT_LT(panel4->GetBounds().width(), panel4->GetRestoredBounds().width());
  EXPECT_NE(docked_position5, panel5->GetBounds().origin());
  EXPECT_LT(panel5->GetBounds().width(), panel5->GetRestoredBounds().width());

#if defined(OS_WIN)
  // The panel we dragged becomes the active one.
  EXPECT_EQ(true, panel3->IsActive());
  EXPECT_EQ(panel3->GetBounds().width(), panel3->GetRestoredBounds().width());

  EXPECT_NE(docked_position6, panel6->GetBounds().origin());
#else
  // The last panel is active so these positions do not change.
  // TODO (ABurago) this is wrong behavior, a panel should activate
  // when it is dragged (it does in real usage, but not when drag is
  // simulated in a test). Change this test when the behavior is fixed.
  EXPECT_EQ(true, panel7->IsActive());
  EXPECT_EQ(panel7->GetBounds().width(), panel7->GetRestoredBounds().width());

  EXPECT_EQ(docked_position6, panel6->GetBounds().origin());
#endif
  EXPECT_EQ(docked_position7, panel7->GetBounds().origin());

  // Drag to attach a detached panel to most-right.
  // Expect to have:
  //   detached:  P1
  //   docked:    P2  P4  P3  P5  P6  P7
  gfx::Point drag_to_location2(panel4->GetBounds().right() + 10,
                               panel4->GetBounds().y());
  DragPanelToMouseLocation(panel2, drag_to_location2);
  ASSERT_EQ(1, detached_collection->num_panels());
  ASSERT_EQ(6, docked_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel2->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel3->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel4->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel5->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel6->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel7->collection()->type());
  EXPECT_EQ(detached_position1, panel1->GetBounds().origin());

  // Drag to attach a detached panel to most-left.
  // Expect to have:
  //   docked:    P2  P4  P1  P3  P5  P6  P7
  gfx::Point drag_to_location3(panel3->GetBounds().x() - 10,
                               panel3->GetBounds().y());
  DragPanelToMouseLocation(panel1, drag_to_location3);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(7, docked_collection->num_panels());
  EXPECT_EQ(PanelCollection::DOCKED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel2->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel3->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel4->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel5->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel6->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel7->collection()->type());

  panel_manager->CloseAll();
}

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_DragDetachedPanelToTop DISABLED_DragDetachedPanelToTop
#else
#define MAYBE_DragDetachedPanelToTop DragDetachedPanelToTop
#endif
IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, MAYBE_DragDetachedPanelToTop) {
  // Setup the test areas to have top-aligned bar excluded from work area.
  const gfx::Rect primary_screen_area(0, 0, 800, 600);
  const gfx::Rect work_area(0, 10, 800, 590);
  SetTestingAreas(primary_screen_area, work_area);

  PanelManager* panel_manager = PanelManager::GetInstance();
  Panel* panel = CreateDetachedPanel("1", gfx::Rect(300, 200, 250, 200));

  // Drag up the panel. Expect that the panel should not go outside the top of
  // the work area.
  gfx::Point drag_to_location(250, 0);
  DragPanelToMouseLocation(panel, drag_to_location);
  EXPECT_EQ(PanelCollection::DETACHED, panel->collection()->type());
  EXPECT_EQ(drag_to_location.x(), panel->GetBounds().origin().x());
  EXPECT_EQ(work_area.y(), panel->GetBounds().origin().y());

  // Drag down the panel. Expect that the panel can be dragged without
  // constraint.
  drag_to_location = gfx::Point(280, 150);
  DragPanelToMouseLocation(panel, drag_to_location);
  EXPECT_EQ(PanelCollection::DETACHED, panel->collection()->type());
  EXPECT_EQ(drag_to_location, panel->GetBounds().origin());

  panel_manager->CloseAll();
}

// TODO(jianli): to be enabled for other platforms when grouping and snapping
// are supported.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, GroupPanelAndPanelFromBottom) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create two detached panels.
  Panel* panel1 = CreateDetachedPanel("1", gfx::Rect(300, 200, 200, 100));
  Panel* panel2 = CreateDetachedPanel("2", gfx::Rect(100, 100, 150, 150));
  ASSERT_EQ(2, detached_collection->num_panels());
  ASSERT_EQ(0, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());

  gfx::Rect panel1_old_bounds = panel1->GetBounds();
  gfx::Rect panel2_old_bounds = panel2->GetBounds();

  // Press on title-bar of P2.
  scoped_ptr<NativePanelTesting> panel2_testing(
      CreateNativePanelTesting(panel2));
  gfx::Point mouse_location(panel2->GetBounds().origin());
  gfx::Point original_mouse_location = mouse_location;
  panel2_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag P2 close to the bottom of P1 to trigger the stacking. Expect:
  // 1) P1 and P2 form a stack.
  // 2) P2 jumps vertcially to align to the bottom edge of P1.
  // 3) P2 moves horizontally by the dragging delta.
  // 4) The width of P2 remains unchanged.
  gfx::Vector2d drag_delta_to_stack =
      GetDragDeltaToStackToBottom(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_stack;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::STACKED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::STACKED, panel2->collection()->type());
  EXPECT_EQ(panel1_old_bounds, panel1->GetBounds());
  gfx::Rect panel2_new_bounds(panel2_old_bounds.x() + drag_delta_to_stack.x(),
                              panel1_old_bounds.bottom(),
                              panel2_old_bounds.width(),
                              panel2_old_bounds.height());
  EXPECT_EQ(panel2_new_bounds, panel2->GetBounds());

  // Drag P2 somewhat away from the bottom of P1 to trigger the unstacking.
  // Expect P1 and P2 become detached.
  gfx::Vector2d drag_delta_to_unstack =
      GetDragDeltaToUnstackFromBottom(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_unstack;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  ASSERT_EQ(0, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());
  EXPECT_EQ(panel1_old_bounds, panel1->GetBounds());
  panel2_new_bounds.set_origin(panel2_old_bounds.origin());
  panel2_new_bounds.Offset(mouse_location - original_mouse_location);
  EXPECT_EQ(panel2_new_bounds, panel2->GetBounds());

  // Drag P2 close to the bottom of P1 to trigger the stacking again. Expect:
  // 1) P1 and P2 form a stack.
  // 2) P2 jumps vertcially to align to the bottom edge of P1.
  // 3) P2 moves horizontally by the dragging delta.
  // 4) The width of P2 remains unchanged.
  drag_delta_to_stack = GetDragDeltaToStackToBottom(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_stack;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::STACKED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::STACKED, panel2->collection()->type());
  EXPECT_EQ(panel1_old_bounds, panel1->GetBounds());
  panel2_new_bounds.set_x(panel2_new_bounds.x() + drag_delta_to_stack.x());
  panel2_new_bounds.set_y(panel1_old_bounds.bottom());
  EXPECT_EQ(panel2_new_bounds, panel2->GetBounds());

  // Move the mouse a little bit. Expect P2 only moves horizontally. P2 should
  // not move vertically since its top edge already glues to the bottom edge
  // of P1.
  gfx::Vector2d small_delta(1, -1);
  mouse_location = mouse_location + small_delta;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::STACKED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::STACKED, panel2->collection()->type());
  EXPECT_EQ(panel1_old_bounds, panel1->GetBounds());
  panel2_new_bounds.set_x(panel2_new_bounds.x() + small_delta.x());
  EXPECT_EQ(panel2_new_bounds, panel2->GetBounds());

  // Finish the drag. Expect:
  // 1) P1 and P2 remain stacked.
  // 2) P2 moves horizontally to align with P1.
  // 3) The width of P2 is adjusted to be same as the one of P1.
  panel2_testing->FinishDragTitlebar();
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::STACKED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::STACKED, panel2->collection()->type());
  EXPECT_EQ(panel1_old_bounds, panel1->GetBounds());
  panel2_new_bounds.set_x(panel1_old_bounds.x());
  panel2_new_bounds.set_width(panel1_old_bounds.width());
  EXPECT_EQ(panel2_new_bounds, panel2->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, GroupPanelAndPanelFromTop) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create two detached panels.
  Panel* panel1 = CreateDetachedPanel("1", gfx::Rect(300, 300, 200, 100));
  Panel* panel2 = CreateDetachedPanel("2", gfx::Rect(100, 200, 150, 150));
  ASSERT_EQ(2, detached_collection->num_panels());
  ASSERT_EQ(0, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());

  gfx::Rect panel1_old_bounds = panel1->GetBounds();
  gfx::Rect panel2_old_bounds = panel2->GetBounds();

  // Press on title-bar of P2.
  scoped_ptr<NativePanelTesting> panel2_testing(
      CreateNativePanelTesting(panel2));
  gfx::Point mouse_location(panel2->GetBounds().origin());
  gfx::Point original_mouse_location = mouse_location;
  panel2_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag P2 close to the top of P1 to trigger the stacking. Expect:
  // 1) P2 and P1 form a stack.
  // 2) P2 jumps vertcially to align to the top edge of P1.
  // 3) P2 moves horizontally by the dragging delta.
  // 4) The width of both P1 and P2 remains unchanged.
  gfx::Vector2d drag_delta_to_stack = GetDragDeltaToStackToTop(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_stack;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::STACKED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::STACKED, panel2->collection()->type());
  EXPECT_EQ(panel1_old_bounds, panel1->GetBounds());
  gfx::Rect panel2_new_bounds(
      panel2_old_bounds.x() + drag_delta_to_stack.x(),
      panel1_old_bounds.y() - panel2_old_bounds.height(),
      panel2_old_bounds.width(),
      panel2_old_bounds.height());
  EXPECT_EQ(panel2_new_bounds, panel2->GetBounds());

  // Drag P2 somewhat away from the top of P1 to trigger the unstacking.
  // Expect P1 and P2 become detached.
  gfx::Vector2d drag_delta_to_unstack =
      GetDragDeltaToUnstackFromTop(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_unstack;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  ASSERT_EQ(0, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());
  EXPECT_EQ(panel1_old_bounds, panel1->GetBounds());
  panel2_new_bounds.set_origin(panel2_old_bounds.origin());
  panel2_new_bounds.Offset(mouse_location - original_mouse_location);
  EXPECT_EQ(panel2_new_bounds, panel2->GetBounds());

  // Drag P2 close to the top of P1 to trigger the stacking again. Expect:
  // 1) P2 and P1 form a stack.
  // 2) P2 jumps vertcially to align to the top edge of P1.
  // 3) P2 moves horizontally by the dragging delta.
  // 4) The width of both P1 and P2 remains unchanged.
  drag_delta_to_stack = GetDragDeltaToStackToTop(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_stack;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::STACKED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::STACKED, panel2->collection()->type());
  EXPECT_EQ(panel1_old_bounds, panel1->GetBounds());
  panel2_new_bounds.set_x(panel2_new_bounds.x() + drag_delta_to_stack.x());
  panel2_new_bounds.set_y(panel1_old_bounds.y() - panel2_old_bounds.height());
  EXPECT_EQ(panel2_new_bounds, panel2->GetBounds());

  // Move the mouse a little bit. Expect only P2 moves horizontally. P2 should
  // not move vertically because its bottom edge already glues to the top edge
  // of P1.
  gfx::Vector2d small_delta(1, -1);
  mouse_location = mouse_location + small_delta;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::STACKED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::STACKED, panel2->collection()->type());
  EXPECT_EQ(panel1_old_bounds, panel1->GetBounds());
  panel2_new_bounds.set_x(panel2_new_bounds.x() + small_delta.x());
  EXPECT_EQ(panel2_new_bounds, panel2->GetBounds());

  // Finish the drag. Expect:
  // 1) P2 and P1 remain stacked.
  // 2) P2 moves horizontally to align with P1.
  // 3) The width of P1 is adjusted to be same as the one of P2.
  panel2_testing->FinishDragTitlebar();
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::STACKED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::STACKED, panel2->collection()->type());
  gfx::Rect panel1_new_bounds = panel1_old_bounds;
  panel1_new_bounds.set_width(panel2_new_bounds.width());
  EXPECT_EQ(panel1_new_bounds, panel1->GetBounds());
  panel2_new_bounds.set_x(panel1_new_bounds.x());
  EXPECT_EQ(panel2_new_bounds, panel2->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, GroupAndCancel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create two detached panels.
  Panel* panel1 = CreateDetachedPanel("1", gfx::Rect(300, 200, 200, 100));
  Panel* panel2 = CreateDetachedPanel("2", gfx::Rect(100, 100, 150, 150));
  ASSERT_EQ(2, detached_collection->num_panels());
  ASSERT_EQ(0, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());

  gfx::Rect panel1_old_bounds = panel1->GetBounds();
  gfx::Rect panel2_old_bounds = panel2->GetBounds();

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel2_testing(
      CreateNativePanelTesting(panel2));
  gfx::Point mouse_location(panel2->GetBounds().origin());
  gfx::Point original_mouse_location = mouse_location;
  panel2_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag P2 close to the bottom of P1 to trigger the stacking.
  // Expect that P2 stacks to P1 and P2's width remains unchanged.
  gfx::Vector2d drag_delta_to_stack =
      GetDragDeltaToStackToBottom(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_stack;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::STACKED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::STACKED, panel2->collection()->type());
  EXPECT_EQ(panel1_old_bounds, panel1->GetBounds());
  gfx::Rect panel2_new_bounds(panel1_old_bounds.x(),
                              panel1_old_bounds.bottom(),
                              panel2_old_bounds.width(),
                              panel2_old_bounds.height());
  EXPECT_EQ(panel2_new_bounds, panel2->GetBounds());

  // Cancel the drag.
  // Expect that the P1 and P2 become detached.
  panel2_testing->CancelDragTitlebar();
  ASSERT_EQ(2, detached_collection->num_panels());
  ASSERT_EQ(0, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());
  EXPECT_EQ(panel1_old_bounds, panel1->GetBounds());
  EXPECT_EQ(panel2_old_bounds, panel2->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, GroupPanelAndStackFromBottom) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 2 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 100, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  ASSERT_EQ(2, stack->num_panels());
  EXPECT_EQ(stack, panel1->collection());
  EXPECT_EQ(stack, panel2->collection());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Create 1 detached panel.
  gfx::Rect panel3_initial_bounds = gfx::Rect(300, 200, 100, 100);
  Panel* panel3 = CreateDetachedPanel("3", panel3_initial_bounds);
  ASSERT_EQ(1, detached_collection->num_panels());
  gfx::Rect panel3_expected_bounds(panel3_initial_bounds);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  // Drag P3 to stack to the bottom of the stack consisting P1 and P2.
  // Expect that P3 becomes the bottom panel of the stack.
  gfx::Vector2d drag_delta_to_stack =
      GetDragDeltaToStackToBottom(panel3, panel2);
  DragPanelByDelta(panel3, drag_delta_to_stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(3, stack->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(stack, panel1->collection());
  EXPECT_EQ(stack, panel2->collection());
  EXPECT_EQ(stack, panel3->collection());

  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel3_expected_bounds = GetStackedAtBottomPanelBounds(
      panel3_initial_bounds, panel2_expected_bounds);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, GroupStackAndPanelFromBottom) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 2 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 100, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  ASSERT_EQ(2, stack->num_panels());
  EXPECT_EQ(stack, panel1->stack());
  EXPECT_EQ(stack, panel2->stack());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Create 1 detached panel.
  gfx::Rect panel3_initial_bounds = gfx::Rect(300, 200, 100, 100);
  Panel* panel3 = CreateDetachedPanel("3", panel3_initial_bounds);
  ASSERT_EQ(1, detached_collection->num_panels());
  gfx::Rect panel3_expected_bounds(panel3_initial_bounds);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  // Drag P1 (together with P2) to stack to the bottom of P3.
  // Expect that P1 and P2 append to the bottom of P3 and all 3 panels are in
  // one stack.
  gfx::Vector2d drag_delta_to_stack =
      GetDragDeltaToStackToBottom(panel1, panel3);
  DragPanelByDelta(panel1, drag_delta_to_stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  StackedPanelCollection* final_stack = panel_manager->stacks().front();
  ASSERT_EQ(3, final_stack->num_panels());
  EXPECT_EQ(final_stack, panel1->stack());
  EXPECT_EQ(final_stack, panel2->stack());
  EXPECT_EQ(final_stack, panel3->stack());

  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());
  panel1_expected_bounds = GetStackedAtBottomPanelBounds(
      panel1_initial_bounds, panel3_expected_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, GroupStackAndPanelFromTop) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 2 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 100, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  ASSERT_EQ(2, stack->num_panels());
  EXPECT_EQ(stack, panel1->stack());
  EXPECT_EQ(stack, panel2->stack());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Create 1 detached panel.
  gfx::Rect panel3_initial_bounds = gfx::Rect(300, 450, 100, 100);
  Panel* panel3 = CreateDetachedPanel("3", panel3_initial_bounds);
  ASSERT_EQ(1, detached_collection->num_panels());
  gfx::Rect panel3_expected_bounds(panel3_initial_bounds);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  // Drag P1 (together with P2) to stack to the top of P3.
  // Expect that P1 and P2 add to the top of P3 and all 3 panels are in
  // one stack. P1 and P2 should align to top of P3 while P3 should update its
  // width to be same as the width of P1 and P2.
  gfx::Vector2d drag_delta_to_stack = GetDragDeltaToStackToTop(panel1, panel3);
  DragPanelByDelta(panel1, drag_delta_to_stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  StackedPanelCollection* final_stack = panel_manager->stacks().front();
  ASSERT_EQ(3, final_stack->num_panels());
  EXPECT_EQ(final_stack, panel1->stack());
  EXPECT_EQ(final_stack, panel2->stack());
  EXPECT_EQ(final_stack, panel3->stack());

  panel3_expected_bounds.set_width(panel1_expected_bounds.width());
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());
  panel2_expected_bounds = GetStackedAtTopPanelBounds(
      panel2_expected_bounds, panel3_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel1_expected_bounds = GetStackedAtTopPanelBounds(
      panel1_expected_bounds, panel2_expected_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, GroupStackAndStackFromBottom) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 2 stacked panels.
  StackedPanelCollection* stack1 = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 50, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack1);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack1);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  ASSERT_EQ(2, stack1->num_panels());
  EXPECT_EQ(stack1, panel1->stack());
  EXPECT_EQ(stack1, panel2->stack());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Create 2 more stacked panels in another stack.
  StackedPanelCollection* stack2 = panel_manager->CreateStack();
  gfx::Rect panel3_initial_bounds = gfx::Rect(300, 100, 220, 120);
  Panel* panel3 = CreateStackedPanel("3", panel3_initial_bounds, stack2);
  gfx::Rect panel4_initial_bounds = gfx::Rect(0, 0, 180, 140);
  Panel* panel4 = CreateStackedPanel("4", panel4_initial_bounds, stack2);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(2, panel_manager->num_stacks());
  ASSERT_EQ(2, stack2->num_panels());
  EXPECT_EQ(stack2, panel3->stack());
  EXPECT_EQ(stack2, panel4->stack());

  gfx::Rect panel3_expected_bounds(panel3_initial_bounds);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());
  gfx::Rect panel4_expected_bounds = GetStackedAtBottomPanelBounds(
      panel4_initial_bounds, panel3_expected_bounds);
  EXPECT_EQ(panel4_expected_bounds, panel4->GetBounds());

  // Drag P3 (together with P4) to stack to the bottom of the stack consisting
  // P1 and P2.
  // Expect that P3 and P4 append to the bottom of P2 and all 4 panels are in
  // one stack.
  gfx::Vector2d drag_delta_to_stack =
      GetDragDeltaToStackToBottom(panel3, panel2);
  DragPanelByDelta(panel3, drag_delta_to_stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  StackedPanelCollection* final_stack = panel_manager->stacks().front();
  ASSERT_EQ(4, final_stack->num_panels());
  EXPECT_EQ(final_stack, panel1->stack());
  EXPECT_EQ(final_stack, panel2->stack());
  EXPECT_EQ(final_stack, panel3->stack());
  EXPECT_EQ(final_stack, panel4->stack());

  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel3_expected_bounds = GetStackedAtBottomPanelBounds(
      panel3_initial_bounds, panel2_expected_bounds);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());
  panel4_expected_bounds = GetStackedAtBottomPanelBounds(
      panel4_initial_bounds, panel3_expected_bounds);
  EXPECT_EQ(panel4_expected_bounds, panel4->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, GroupStackAndStackFromTop) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 2 stacked panels.
  StackedPanelCollection* stack1 = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 50, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack1);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack1);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  ASSERT_EQ(2, stack1->num_panels());
  EXPECT_EQ(stack1, panel1->stack());
  EXPECT_EQ(stack1, panel2->stack());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Create 2 more stacked panels in another stack.
  StackedPanelCollection* stack2 = panel_manager->CreateStack();
  gfx::Rect panel3_initial_bounds = gfx::Rect(300, 350, 220, 110);
  Panel* panel3 = CreateStackedPanel("3", panel3_initial_bounds, stack2);
  gfx::Rect panel4_initial_bounds = gfx::Rect(0, 0, 180, 100);
  Panel* panel4 = CreateStackedPanel("4", panel4_initial_bounds, stack2);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(2, panel_manager->num_stacks());
  ASSERT_EQ(2, stack2->num_panels());
  EXPECT_EQ(stack2, panel3->stack());
  EXPECT_EQ(stack2, panel4->stack());

  gfx::Rect panel3_expected_bounds(panel3_initial_bounds);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());
  gfx::Rect panel4_expected_bounds = GetStackedAtBottomPanelBounds(
      panel4_initial_bounds, panel3_expected_bounds);
  EXPECT_EQ(panel4_expected_bounds, panel4->GetBounds());

  // Drag P1 (together with P2) to stack to the top of the stack consisting
  // P3 and P4.
  // Expect that P1 and P2 add to the top of P3 and all 4 panels are in
  // one stack. P1 and P2 should align to top of P3 while P3 and P4 should
  // update their width to be same as the width of P1 and P2.
  gfx::Vector2d drag_delta_to_stack = GetDragDeltaToStackToTop(panel1, panel3);
  DragPanelByDelta(panel1, drag_delta_to_stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  StackedPanelCollection* final_stack = panel_manager->stacks().front();
  ASSERT_EQ(4, final_stack->num_panels());
  EXPECT_EQ(final_stack, panel1->stack());
  EXPECT_EQ(final_stack, panel2->stack());
  EXPECT_EQ(final_stack, panel3->stack());
  EXPECT_EQ(final_stack, panel4->stack());

  panel4_expected_bounds.set_width(panel1_expected_bounds.width());
  EXPECT_EQ(panel4_expected_bounds, panel4->GetBounds());
  panel3_expected_bounds.set_width(panel1_expected_bounds.width());
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());
  panel2_expected_bounds = GetStackedAtTopPanelBounds(
      panel2_expected_bounds, panel3_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel1_expected_bounds = GetStackedAtTopPanelBounds(
      panel1_expected_bounds, panel2_expected_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, UngroupTwoPanelStack) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 2 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 50, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(2, stack->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(stack, panel1->stack());
  EXPECT_EQ(stack, panel2->stack());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  gfx::Rect panel2_old_bounds = panel2_expected_bounds;

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel2_testing(
      CreateNativePanelTesting(panel2));
  gfx::Point mouse_location(panel2->GetBounds().origin());
  gfx::Point original_mouse_location = mouse_location;
  panel2_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag P2 away from the bottom of P1 to trigger the unstacking.
  // Expect that P1 and P2 get detached.
  gfx::Vector2d drag_delta_to_unstack =
      GetDragDeltaToUnstackFromBottom(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_unstack;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  ASSERT_TRUE(stack->num_panels() == 0);
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.Offset(drag_delta_to_unstack);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Drag P2 a bit closer to the bottom of P1 to trigger the stacking.
  // Expect P1 and P2 get stacked together.
  gfx::Vector2d drag_delta_to_stack =
      GetDragDeltaToStackToBottom(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_stack;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, detached_collection->num_panels());
  // Note that the empty stack might still exist until the drag ends.
  ASSERT_GE(panel_manager->num_stacks(), 1);
  EXPECT_EQ(PanelCollection::STACKED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::STACKED, panel2->collection()->type());
  EXPECT_EQ(panel1->stack(), panel2->stack());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Drag P2 away from the bottom of P1 to trigger the unstacking again.
  // Expect that P1 and P2 get detached.
  drag_delta_to_unstack = GetDragDeltaToUnstackFromBottom(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_unstack;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  ASSERT_TRUE(stack->num_panels() == 0);
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds = panel2_old_bounds;
  panel2_expected_bounds.Offset(mouse_location - original_mouse_location);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Finish the drag.
  // Expect that the P1 and P2 stay detached.
  panel2_testing->FinishDragTitlebar();
  ASSERT_EQ(2, detached_collection->num_panels());
  ASSERT_EQ(0, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, UngroupAndCancel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 2 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 50, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(2, stack->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(stack, panel1->stack());
  EXPECT_EQ(stack, panel2->stack());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  gfx::Rect panel2_old_bounds = panel2->GetBounds();

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel2_testing(
      CreateNativePanelTesting(panel2));
  gfx::Point mouse_location(panel2->GetBounds().origin());
  gfx::Point original_mouse_location = mouse_location;
  panel2_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag P2 away from the bottom of P1 to trigger the unstacking.
  // Expect that P1 and P2 get detached.
  gfx::Vector2d drag_delta_to_unstack =
      GetDragDeltaToUnstackFromBottom(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_unstack;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  ASSERT_TRUE(stack->num_panels() == 0);
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.Offset(drag_delta_to_unstack);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Cancel the drag.
  // Expect that the P1 and P2 put back to the same stack.
  panel2_testing->CancelDragTitlebar();
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::STACKED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::STACKED, panel2->collection()->type());
  EXPECT_EQ(stack, panel1->stack());
  EXPECT_EQ(stack, panel2->stack());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  EXPECT_EQ(panel2_old_bounds, panel2->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest,
                       UngroupBottomPanelInThreePanelStack) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 3 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 50, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);
  gfx::Rect panel3_initial_bounds = gfx::Rect(0, 0, 120, 120);
  Panel* panel3 = CreateStackedPanel("3", panel3_initial_bounds, stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  ASSERT_EQ(3, stack->num_panels());
  EXPECT_EQ(stack, panel1->stack());
  EXPECT_EQ(stack, panel2->stack());
  EXPECT_EQ(stack, panel3->stack());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  gfx::Rect panel3_expected_bounds = GetStackedAtBottomPanelBounds(
      panel3_initial_bounds, panel2_expected_bounds);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  // Drag P3 away to unstack from P2 and P1.
  // Expect that P1 and P2 are still in the stack while P3 gets detached.
  gfx::Vector2d drag_delta_to_unstack =
      GetDragDeltaToUnstackFromBottom(panel3, panel2);
  DragPanelByDelta(panel3, drag_delta_to_unstack);
  ASSERT_EQ(1, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  ASSERT_EQ(2, stack->num_panels());
  EXPECT_EQ(stack, panel1->stack());
  EXPECT_EQ(stack, panel2->stack());
  EXPECT_EQ(PanelCollection::DETACHED, panel3->collection()->type());

  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel3_expected_bounds.Offset(drag_delta_to_unstack);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest,
                       UngroupMiddlePanelInThreePanelStack) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 3 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 50, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);
  gfx::Rect panel3_initial_bounds = gfx::Rect(0, 0, 120, 120);
  Panel* panel3 = CreateStackedPanel("3", panel3_initial_bounds, stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  ASSERT_EQ(3, stack->num_panels());
  EXPECT_EQ(stack, panel1->stack());
  EXPECT_EQ(stack, panel2->stack());
  EXPECT_EQ(stack, panel3->stack());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  gfx::Rect panel3_expected_bounds = GetStackedAtBottomPanelBounds(
      panel3_initial_bounds, panel2_expected_bounds);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  // Drag P2 (together with P3) away to unstack from P1.
  // Expect that P2 and P3 are still in a stack while P1 gets detached.
  gfx::Vector2d drag_delta_to_unstack =
      GetDragDeltaToUnstackFromBottom(panel2, panel1);
  DragPanelByDelta(panel2, drag_delta_to_unstack);
  ASSERT_EQ(1, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  StackedPanelCollection* final_stack = panel_manager->stacks().front();
  ASSERT_EQ(2, final_stack->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(final_stack, panel2->stack());
  EXPECT_EQ(final_stack, panel3->stack());

  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.Offset(drag_delta_to_unstack);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel3_expected_bounds.Offset(drag_delta_to_unstack);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest,
                       UngroupThirdPanelInFourPanelStack) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 4 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 120, 200, 100);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);
  gfx::Rect panel3_initial_bounds = gfx::Rect(0, 0, 120, 120);
  Panel* panel3 = CreateStackedPanel("3", panel3_initial_bounds, stack);
  gfx::Rect panel4_initial_bounds = gfx::Rect(0, 0, 120, 110);
  Panel* panel4 = CreateStackedPanel("4", panel4_initial_bounds, stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  ASSERT_EQ(4, stack->num_panels());
  EXPECT_EQ(stack, panel1->stack());
  EXPECT_EQ(stack, panel2->stack());
  EXPECT_EQ(stack, panel3->stack());
  EXPECT_EQ(stack, panel4->stack());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  gfx::Rect panel3_expected_bounds = GetStackedAtBottomPanelBounds(
      panel3_initial_bounds, panel2_expected_bounds);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());
  gfx::Rect panel4_expected_bounds = GetStackedAtBottomPanelBounds(
      panel4_initial_bounds, panel3_expected_bounds);
  EXPECT_EQ(panel4_expected_bounds, panel4->GetBounds());

  // Drag P3 (together with P4) away to unstack from P2.
  // Expect that P1 and P2 are in one stack while P3 and P4 are in different
  // stack.
  gfx::Vector2d drag_delta_to_unstack =
      GetDragDeltaToUnstackFromBottom(panel3, panel2);
  DragPanelByDelta(panel3, drag_delta_to_unstack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(2, panel_manager->num_stacks());
  StackedPanelCollection* final_stack1 = panel_manager->stacks().front();
  ASSERT_EQ(2, final_stack1->num_panels());
  StackedPanelCollection* final_stack2 = panel_manager->stacks().back();
  ASSERT_EQ(2, final_stack2->num_panels());
  EXPECT_EQ(panel1->stack(), panel2->stack());
  EXPECT_EQ(panel3->stack(), panel4->stack());

  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel3_expected_bounds.Offset(drag_delta_to_unstack);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());
  panel4_expected_bounds.Offset(drag_delta_to_unstack);
  EXPECT_EQ(panel4_expected_bounds, panel4->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, UngroupAndGroup) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 2 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 50, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(2, stack->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(stack, panel1->stack());
  EXPECT_EQ(stack, panel2->stack());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Create 1 detached panel.
  gfx::Rect panel3_initial_bounds = gfx::Rect(300, 200, 100, 100);
  Panel* panel3 = CreateDetachedPanel("3", panel3_initial_bounds);
  ASSERT_EQ(1, detached_collection->num_panels());
  gfx::Rect panel3_expected_bounds(panel3_initial_bounds);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  // Drag P2 to the bottom edge of P3 to trigger both unstacking and stacking.
  // Expect that P3 and P2 are stacked together while P1 gets detached.
  gfx::Vector2d drag_delta_to_unstack_and_stack =
      GetDragDeltaToStackToBottom(panel2, panel3);
  DragPanelByDelta(panel2, drag_delta_to_unstack_and_stack);
  ASSERT_EQ(1, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  StackedPanelCollection* final_stack = panel_manager->stacks().front();
  ASSERT_EQ(2, final_stack->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(final_stack, panel2->stack());
  EXPECT_EQ(final_stack, panel3->stack());

  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel3_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, UngroupAndAttach) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelCollection* docked_collection = panel_manager->docked_collection();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 2 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 50, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);
  ASSERT_EQ(0, docked_collection->num_panels());
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(2, stack->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(stack, panel1->stack());
  EXPECT_EQ(stack, panel2->stack());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Drag P2 close to the bottom of the work area to trigger both unstacking and
  // docking for P2.
  // Expect that P2 gets docked while P2 gets detached.
  gfx::Vector2d drag_delta_to_unstack_and_attach = GetDragDeltaToAttach(panel2);
  DragPanelByDelta(panel2, drag_delta_to_unstack_and_attach);
  WaitForBoundsAnimationFinished(panel2);
  ASSERT_EQ(1, docked_collection->num_panels());
  ASSERT_EQ(1, detached_collection->num_panels());
  ASSERT_EQ(0, panel_manager->num_stacks());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DOCKED, panel2->collection()->type());

  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.set_x(docked_collection->StartingRightPosition() -
      panel2_expected_bounds.width());
  panel2_expected_bounds.set_y(docked_collection->display_area().bottom() -
      panel2_expected_bounds.height());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, SnapPanelToPanelLeft) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 2 detached panels.
  gfx::Rect panel1_initial_bounds = gfx::Rect(300, 200, 300, 200);
  Panel* panel1 = CreateDetachedPanel("1", panel1_initial_bounds);
  gfx::Rect panel2_initial_bounds = gfx::Rect(100, 100, 100, 250);
  Panel* panel2 = CreateDetachedPanel("2", panel2_initial_bounds);
  ASSERT_EQ(2, detached_collection->num_panels());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds(panel2_initial_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel2_testing(
      CreateNativePanelTesting(panel2));
  gfx::Point mouse_location(panel2->GetBounds().origin());
  gfx::Point original_mouse_location = mouse_location;
  panel2_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag P2 close to the left of P1 to trigger the snapping.
  gfx::Vector2d drag_delta_to_snap = GetDragDeltaToSnapToLeft(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_snap;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.Offset(drag_delta_to_snap);
  panel2_expected_bounds.set_x(
      panel1_expected_bounds.x() - panel2_expected_bounds.width());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Drag P2 a bit away from the left of P1 to trigger the unsnapping.
  gfx::Vector2d drag_delta_to_unsnap = GetDragDeltaToUnsnap(panel1);
  mouse_location = mouse_location + drag_delta_to_unsnap;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds = panel2_initial_bounds;
  panel2_expected_bounds.Offset(mouse_location - original_mouse_location);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Drag P2 close to the left of P1 to trigger the snapping again.
  drag_delta_to_snap = GetDragDeltaToSnapToLeft(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_snap;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.Offset(drag_delta_to_snap);
  panel2_expected_bounds.set_x(
      panel1_expected_bounds.x() - panel2_expected_bounds.width());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Drag P2 vertically with a little bit of horizontal movement should still
  // keep the snapping.
  gfx::Vector2d drag_delta_almost_vertically(2, 20);
  mouse_location = mouse_location + drag_delta_almost_vertically;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.set_y(
      panel2_expected_bounds.y() + drag_delta_almost_vertically.y());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Finish the drag.
  panel2_testing->FinishDragTitlebar();
  ASSERT_EQ(2, detached_collection->num_panels());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, SnapPanelToPanelRight) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 2 detached panels.
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 200, 100, 200);
  Panel* panel1 = CreateDetachedPanel("1", panel1_initial_bounds);
  gfx::Rect panel2_initial_bounds = gfx::Rect(300, 100, 200, 250);
  Panel* panel2 = CreateDetachedPanel("2", panel2_initial_bounds);
  ASSERT_EQ(2, detached_collection->num_panels());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds(panel2_initial_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel2_testing(
      CreateNativePanelTesting(panel2));
  gfx::Point mouse_location(panel2->GetBounds().origin());
  gfx::Point original_mouse_location = mouse_location;
  panel2_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag P1 close to the right of P2 to trigger the snapping.
  gfx::Vector2d drag_delta_to_snap = GetDragDeltaToSnapToRight(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_snap;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.Offset(drag_delta_to_snap);
  panel2_expected_bounds.set_x(panel1_expected_bounds.right());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Drag P2 a bit away from the right of P1 to trigger the unsnapping.
  gfx::Vector2d drag_delta_to_unsnap = GetDragDeltaToUnsnap(panel1);
  mouse_location = mouse_location + drag_delta_to_unsnap;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds = panel2_initial_bounds;
  panel2_expected_bounds.Offset(mouse_location - original_mouse_location);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Drag P2 close to the right of P1 to trigger the snapping again.
  drag_delta_to_snap = GetDragDeltaToSnapToRight(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_snap;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.Offset(drag_delta_to_snap);
  panel2_expected_bounds.set_x(panel1_expected_bounds.right());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Drag P2 vertically with a little bit of horizontal movement should still
  // keep the snapping.
  gfx::Vector2d drag_delta_almost_vertically(2, -20);
  mouse_location = mouse_location + drag_delta_almost_vertically;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.set_y(
      panel2_expected_bounds.y() + drag_delta_almost_vertically.y());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Finish the drag.
  panel2_testing->FinishDragTitlebar();
  ASSERT_EQ(2, detached_collection->num_panels());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, SnapAndCancel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 2 detached panels.
  gfx::Rect panel1_initial_bounds = gfx::Rect(300, 200, 300, 200);
  Panel* panel1 = CreateDetachedPanel("1", panel1_initial_bounds);
  gfx::Rect panel2_initial_bounds = gfx::Rect(100, 100, 100, 250);
  Panel* panel2 = CreateDetachedPanel("2", panel2_initial_bounds);
  ASSERT_EQ(2, detached_collection->num_panels());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds(panel2_initial_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel2_testing(
      CreateNativePanelTesting(panel2));
  gfx::Point mouse_location(panel2->GetBounds().origin());
  gfx::Point original_mouse_location = mouse_location;
  panel2_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag P2 close to the left of P1 to trigger the snapping.
  gfx::Vector2d drag_delta_to_snap = GetDragDeltaToSnapToLeft(panel2, panel1);
  mouse_location = mouse_location + drag_delta_to_snap;
  panel2_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(2, detached_collection->num_panels());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.Offset(drag_delta_to_snap);
  panel2_expected_bounds.set_x(
      panel1_expected_bounds.x() - panel2_expected_bounds.width());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Cancel the drag.
  panel2_testing->CancelDragTitlebar();
  ASSERT_EQ(2, detached_collection->num_panels());
  EXPECT_EQ(panel1_initial_bounds, panel1->GetBounds());
  EXPECT_EQ(panel2_initial_bounds, panel2->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, SnapPanelToStackLeft) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 2 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(300, 100, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  ASSERT_EQ(2, stack->num_panels());
  EXPECT_EQ(stack, panel1->collection());
  EXPECT_EQ(stack, panel2->collection());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Create 1 detached panel.
  gfx::Rect panel3_initial_bounds = gfx::Rect(100, 200, 100, 100);
  Panel* panel3 = CreateDetachedPanel("3", panel3_initial_bounds);
  ASSERT_EQ(1, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel3->collection()->type());
  gfx::Rect panel3_expected_bounds(panel3_initial_bounds);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  // Drag P3 close to the left of the stack of P1 and P2 to trigger the
  // snapping.
  gfx::Vector2d drag_delta_to_snap = GetDragDeltaToSnapToLeft(panel3, panel1);
  DragPanelByDelta(panel3, drag_delta_to_snap);
  ASSERT_EQ(1, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  ASSERT_EQ(2, stack->num_panels());
  EXPECT_EQ(stack, panel1->collection());
  EXPECT_EQ(stack, panel2->collection());
  EXPECT_EQ(PanelCollection::DETACHED, panel3->collection()->type());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel3_expected_bounds.Offset(drag_delta_to_snap);
  panel3_expected_bounds.set_x(
      panel1_expected_bounds.x() - panel3_expected_bounds.width());
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, SnapPanelToStackRight) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 2 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 100, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);
  ASSERT_EQ(0, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  ASSERT_EQ(2, stack->num_panels());
  EXPECT_EQ(stack, panel1->collection());
  EXPECT_EQ(stack, panel2->collection());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Create 1 detached panel.
  gfx::Rect panel3_initial_bounds = gfx::Rect(300, 200, 100, 100);
  Panel* panel3 = CreateDetachedPanel("3", panel3_initial_bounds);
  ASSERT_EQ(1, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel3->collection()->type());
  gfx::Rect panel3_expected_bounds(panel3_initial_bounds);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  // Drag P3 close to the right of the stack of P1 and P2 to trigger the
  // snapping.
  gfx::Vector2d drag_delta_to_snap = GetDragDeltaToSnapToRight(panel3, panel1);
  DragPanelByDelta(panel3, drag_delta_to_snap);
  ASSERT_EQ(1, detached_collection->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  ASSERT_EQ(2, stack->num_panels());
  EXPECT_EQ(stack, panel1->collection());
  EXPECT_EQ(stack, panel2->collection());
  EXPECT_EQ(PanelCollection::DETACHED, panel3->collection()->type());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel3_expected_bounds.Offset(drag_delta_to_snap);
  panel3_expected_bounds.set_x(panel1_expected_bounds.right());
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, DetachAndSnap) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelCollection* docked_collection = panel_manager->docked_collection();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 1 detached panel.
  Panel* panel1 = CreateDetachedPanel("1", gfx::Rect(300, 200, 100, 100));
  ASSERT_EQ(0, docked_collection->num_panels());
  ASSERT_EQ(1, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  gfx::Rect panel1_bounds = panel1->GetBounds();

  // Create 1 docked panel.
  Panel* panel2 = CreateDockedPanel("2", gfx::Rect(0, 0, 150, 150));
  ASSERT_EQ(1, docked_collection->num_panels());
  ASSERT_EQ(1, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DOCKED, panel2->collection()->type());
  gfx::Rect panel2_bounds = panel2->GetBounds();

  // Drag P2 close to the right of P1 to trigger both detaching and snapping.
  gfx::Vector2d drag_delta_to_detach_and_snap =
      GetDragDeltaToSnapToRight(panel2, panel1);
  DragPanelByDelta(panel2, drag_delta_to_detach_and_snap);
  ASSERT_EQ(0, docked_collection->num_panels());
  ASSERT_EQ(2, detached_collection->num_panels());
  EXPECT_EQ(PanelCollection::DETACHED, panel1->collection()->type());
  EXPECT_EQ(PanelCollection::DETACHED, panel2->collection()->type());
  EXPECT_EQ(panel1_bounds, panel1->GetBounds());
  panel2_bounds.Offset(drag_delta_to_detach_and_snap);
  panel2_bounds.set_x(panel1_bounds.right());
  EXPECT_EQ(panel2_bounds, panel2->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, DragTopStackedPanel) {
  PanelManager* panel_manager = PanelManager::GetInstance();

  // Create 3 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(300, 100, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);
  gfx::Rect panel3_initial_bounds = gfx::Rect(0, 0, 150, 200);
  Panel* panel3 = CreateStackedPanel("3", panel3_initial_bounds, stack);
  ASSERT_EQ(3, stack->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(stack, panel1->collection());
  EXPECT_EQ(stack, panel2->collection());
  EXPECT_EQ(stack, panel3->collection());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  gfx::Rect panel3_expected_bounds = GetStackedAtBottomPanelBounds(
      panel3_initial_bounds, panel2_expected_bounds);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  // Drag the top panel by a delta.
  // Expect all panels are still in the same stack and they are all moved by the
  // same delta.
  gfx::Vector2d drag_delta(-50, -20);
  DragPanelByDelta(panel1, drag_delta);
  ASSERT_EQ(3, stack->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  EXPECT_EQ(stack, panel1->collection());
  EXPECT_EQ(stack, panel2->collection());
  EXPECT_EQ(stack, panel3->collection());

  panel1_expected_bounds.Offset(drag_delta);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.Offset(drag_delta);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel3_expected_bounds.Offset(drag_delta);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, SnapDetachedPanelToScreenEdge) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  gfx::Rect display_area = panel_manager->display_area();
  int small_distance =
      PanelDragController::GetSnapPanelToScreenEdgeThresholdForTesting() / 2;

  // Create one detached panel.
  gfx::Rect panel_initial_bounds = gfx::Rect(300, 100, 200, 150);
  Panel* panel = CreateDetachedPanel("1", panel_initial_bounds);
  gfx::Rect panel_expected_bounds(panel_initial_bounds);
  EXPECT_EQ(panel_expected_bounds, panel->GetBounds());

  // Drag the panel close to the right screen edge.
  // Expect that the panel should snap to the screen edge.
  gfx::Vector2d drag_delta(
      display_area.right() - small_distance - panel->GetBounds().right(), 0);
  DragPanelByDelta(panel, drag_delta);
  panel_expected_bounds.set_x(
      display_area.right() - panel->GetBounds().width());
  EXPECT_EQ(panel_expected_bounds, panel->GetBounds());

  // Drag the panel close to the top-left corner of the screen.
  // Expect that the panel should snap to the screen corner.
  gfx::Vector2d drag_delta2(
      display_area.x() + small_distance - panel->GetBounds().x(),
      display_area.y() - small_distance - panel->GetBounds().y());
  DragPanelByDelta(panel, drag_delta2);
  panel_expected_bounds.set_origin(display_area.origin());
  EXPECT_EQ(panel_expected_bounds, panel->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, SnapStackedPanelToScreenEdge) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  gfx::Rect display_area = panel_manager->display_area();
  int small_distance =
      PanelDragController::GetSnapPanelToScreenEdgeThresholdForTesting() / 2;

  // Create 2 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(300, 100, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds = GetStackedAtBottomPanelBounds(
      panel2_initial_bounds, panel1_expected_bounds);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Drag the stack close to the left screen edge.
  // Expect that the stack should snap to the screen edge.
  gfx::Vector2d drag_delta(
      display_area.x() + small_distance - panel1->GetBounds().x(), 0);
  DragPanelByDelta(panel1, drag_delta);

  panel1_expected_bounds.set_x(display_area.x());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.set_x(display_area.x());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  // Drag the stack close to the bottom-right corner of the screen.
  // Expect that the stack should snap to the screen corner.
  gfx::Vector2d drag_delta2(
      display_area.right() + small_distance - panel1->GetBounds().right(),
      display_area.bottom() - small_distance - panel1->GetBounds().bottom() -
          panel2->GetBounds().height());
  DragPanelByDelta(panel1, drag_delta2);

  int expected_x = display_area.right() - panel1->GetBounds().width();
  panel1_expected_bounds.set_x(expected_x);
  panel1_expected_bounds.set_y(display_area.bottom() -
      panel1->GetBounds().height() - panel2->GetBounds().height());
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.set_x(expected_x);
  panel2_expected_bounds.set_y(display_area.bottom() -
      panel2->GetBounds().height());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());

  panel_manager->CloseAll();
}
#endif
