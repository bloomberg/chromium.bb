// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/detached_panel_strip.h"
#include "chrome/browser/ui/panels/docked_panel_strip.h"
#include "chrome/browser/ui/panels/overflow_panel_strip.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_drag_controller.h"
#include "chrome/browser/ui/panels/panel_manager.h"

class PanelDragBrowserTest : public BasePanelBrowserTest {
 public:
  PanelDragBrowserTest() : BasePanelBrowserTest() {
  }

  virtual ~PanelDragBrowserTest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    BasePanelBrowserTest::SetUpOnMainThread();

    // All the tests here assume 800x600 work area. Do the check now.
    DCHECK(PanelManager::GetInstance()->work_area().width() == 800);
    DCHECK(PanelManager::GetInstance()->work_area().height() == 600);
  }

  // Drag |panel| from its origin by the offset |delta|.
  void DragPanelByDelta(Panel* panel, const gfx::Point& delta) {
    scoped_ptr<NativePanelTesting> panel_testing(
        NativePanelTesting::Create(panel->native_panel()));
    gfx::Point mouse_location(panel->GetBounds().origin());
    panel_testing->PressLeftMouseButtonTitlebar(mouse_location);
    panel_testing->DragTitlebar(mouse_location.Add(delta));
    panel_testing->FinishDragTitlebar();
  }

  // Drag |panel| from its origin to |new_mouse_location|.
  void DragPanelToMouseLocation(Panel* panel,
                                const gfx::Point& new_mouse_location) {
    scoped_ptr<NativePanelTesting> panel_testing(
        NativePanelTesting::Create(panel->native_panel()));
    gfx::Point mouse_location(panel->GetBounds().origin());
    panel_testing->PressLeftMouseButtonTitlebar(panel->GetBounds().origin());
    panel_testing->DragTitlebar(new_mouse_location);
    panel_testing->FinishDragTitlebar();
  }

  static gfx::Point GetDragDeltaToRemainDocked() {
    return gfx::Point(
        -5,
        -(PanelDragController::GetDetachDockedPanelThreshold() / 2));
  }

  static gfx::Point GetDragDeltaToDetach() {
    return gfx::Point(
        -20,
        -(PanelDragController::GetDetachDockedPanelThreshold() + 20));
  }

  static gfx::Point GetDragDeltaToRemainDetached(Panel* panel) {
    int distance = panel->manager()->docked_strip()->display_area().bottom() -
                   panel->GetBounds().bottom();
    return gfx::Point(
        -5,
        distance - PanelDragController::GetDockDetachedPanelThreshold() * 2);
  }

  static gfx::Point GetDragDeltaToAttach(Panel* panel) {
    int distance = panel->manager()->docked_strip()->display_area().bottom() -
                   panel->GetBounds().bottom();
    return gfx::Point(
        -20,
        distance - PanelDragController::GetDockDetachedPanelThreshold() / 2);
  }
};

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, Detach) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();
  DetachedPanelStrip* detached_strip = panel_manager->detached_strip();

  // Create one docked panel.
  Panel* panel = CreateDockedPanel("1", gfx::Rect(0, 0, 100, 100));
  ASSERT_EQ(1, docked_strip->num_panels());
  ASSERT_EQ(0, detached_strip->num_panels());

  gfx::Rect panel_old_bounds = panel->GetBounds();

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel_testing(
      NativePanelTesting::Create(panel->native_panel()));
  gfx::Point mouse_location(panel->GetBounds().origin());
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag up the panel in a small offset that does not trigger the detach.
  // Expect that the panel is still docked and only x coordinate of its position
  // is changed.
  gfx::Point drag_delta_to_remain_docked = GetDragDeltaToRemainDocked();
  mouse_location = mouse_location.Add(drag_delta_to_remain_docked);
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(1, docked_strip->num_panels());
  ASSERT_EQ(0, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DOCKED, panel->panel_strip()->type());
  gfx::Rect panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(drag_delta_to_remain_docked.x(), 0);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Continue dragging up the panel in big offset that triggers the detach.
  // Expect that the panel is previewed as detached.
  gfx::Point drag_delta_to_detach = GetDragDeltaToDetach();
  mouse_location = mouse_location.Add(drag_delta_to_detach);
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, docked_strip->num_panels());
  ASSERT_EQ(1, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DETACHED, panel->panel_strip()->type());
  panel_new_bounds.Offset(
      drag_delta_to_detach.x(),
      drag_delta_to_detach.y() + drag_delta_to_remain_docked.y());
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Finish the drag.
  // Expect that the panel stays as detached.
  panel_testing->FinishDragTitlebar();
  ASSERT_EQ(0, docked_strip->num_panels());
  ASSERT_EQ(1, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DETACHED, panel->panel_strip()->type());
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, DetachAndCancel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();
  DetachedPanelStrip* detached_strip = panel_manager->detached_strip();

  // Create one docked panel.
  Panel* panel = CreateDockedPanel("1", gfx::Rect(0, 0, 100, 100));
  ASSERT_EQ(1, docked_strip->num_panels());
  ASSERT_EQ(0, detached_strip->num_panels());

  gfx::Rect panel_old_bounds = panel->GetBounds();

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel_testing(
      NativePanelTesting::Create(panel->native_panel()));
  gfx::Point mouse_location(panel->GetBounds().origin());
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag up the panel in a small offset that does not trigger the detach.
  // Expect that the panel is still docked and only x coordinate of its position
  // is changed.
  gfx::Point drag_delta_to_remain_docked = GetDragDeltaToRemainDocked();
  mouse_location = mouse_location.Add(drag_delta_to_remain_docked);
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(1, docked_strip->num_panels());
  ASSERT_EQ(0, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DOCKED, panel->panel_strip()->type());
  gfx::Rect panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(drag_delta_to_remain_docked.x(), 0);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Continue dragging up the panel in big offset that triggers the detach.
  // Expect that the panel is previewed as detached.
  gfx::Point drag_delta_to_detach = GetDragDeltaToDetach();
  mouse_location = mouse_location.Add(drag_delta_to_detach);
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, docked_strip->num_panels());
  ASSERT_EQ(1, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DETACHED, panel->panel_strip()->type());
  panel_new_bounds.Offset(
      drag_delta_to_detach.x(),
      drag_delta_to_detach.y() + drag_delta_to_remain_docked.y());
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Cancel the drag.
  // Expect that the panel is back as docked.
  panel_testing->CancelDragTitlebar();
  ASSERT_EQ(1, docked_strip->num_panels());
  ASSERT_EQ(0, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DOCKED, panel->panel_strip()->type());
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, Attach) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();
  DetachedPanelStrip* detached_strip = panel_manager->detached_strip();

  // Create one detached panel.
  Panel* panel = CreateDetachedPanel("1", gfx::Rect(400, 300, 100, 100));
  ASSERT_EQ(0, docked_strip->num_panels());
  ASSERT_EQ(1, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DETACHED, panel->panel_strip()->type());

  gfx::Rect panel_old_bounds = panel->GetBounds();

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel_testing(
      NativePanelTesting::Create(panel->native_panel()));
  gfx::Point mouse_location(panel->GetBounds().origin());
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag down the panel but not close enough to the bottom of work area.
  // Expect that the panel is still detached.
  gfx::Point drag_delta_to_remain_detached =
      GetDragDeltaToRemainDetached(panel);
  mouse_location = mouse_location.Add(drag_delta_to_remain_detached);
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, docked_strip->num_panels());
  ASSERT_EQ(1, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DETACHED, panel->panel_strip()->type());
  gfx::Rect panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(drag_delta_to_remain_detached);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Continue dragging down the panel to make it close enough to the bottom of
  // work area.
  // Expect that the panel is previewed as docked.
  gfx::Point drag_delta_to_attach = GetDragDeltaToAttach(panel);
  mouse_location = mouse_location.Add(drag_delta_to_attach);
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(1, docked_strip->num_panels());
  ASSERT_EQ(0, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DOCKED, panel->panel_strip()->type());
  panel_new_bounds.Offset(drag_delta_to_attach);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Finish the drag.
  // Expect that the panel stays as docked and moves to the final position.
  panel_testing->FinishDragTitlebar();
  ASSERT_EQ(1, docked_strip->num_panels());
  ASSERT_EQ(0, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DOCKED, panel->panel_strip()->type());
  panel_new_bounds.set_x(
      docked_strip->StartingRightPosition() - panel_new_bounds.width());
  panel_new_bounds.set_y(
      docked_strip->display_area().bottom() - panel_new_bounds.height());
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, AttachAndCancel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();
  DetachedPanelStrip* detached_strip = panel_manager->detached_strip();

  // Create one detached panel.
  Panel* panel = CreateDetachedPanel("1", gfx::Rect(400, 300, 100, 100));
  ASSERT_EQ(0, docked_strip->num_panels());
  ASSERT_EQ(1, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DETACHED, panel->panel_strip()->type());

  gfx::Rect panel_old_bounds = panel->GetBounds();

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel_testing(
      NativePanelTesting::Create(panel->native_panel()));
  gfx::Point mouse_location(panel->GetBounds().origin());
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag down the panel but not close enough to the bottom of work area.
  // Expect that the panel is still detached.
  gfx::Point drag_delta_to_remain_detached =
      GetDragDeltaToRemainDetached(panel);
  mouse_location = mouse_location.Add(drag_delta_to_remain_detached);
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, docked_strip->num_panels());
  ASSERT_EQ(1, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DETACHED, panel->panel_strip()->type());
  gfx::Rect panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(drag_delta_to_remain_detached);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Continue dragging down the panel to make it close enough to the bottom of
  // work area.
  // Expect that the panel is previewed as docked.
  gfx::Point drag_delta_to_attach = GetDragDeltaToAttach(panel);
  mouse_location = mouse_location.Add(drag_delta_to_attach);
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(1, docked_strip->num_panels());
  ASSERT_EQ(0, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DOCKED, panel->panel_strip()->type());
  panel_new_bounds.Offset(drag_delta_to_attach);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Cancel the drag.
  // Expect that the panel is back as detached.
  panel_testing->CancelDragTitlebar();
  ASSERT_EQ(0, docked_strip->num_panels());
  ASSERT_EQ(1, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DETACHED, panel->panel_strip()->type());
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, DetachAttachAndCancel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();
  DetachedPanelStrip* detached_strip = panel_manager->detached_strip();

  // Create one docked panel.
  Panel* panel = CreateDockedPanel("1", gfx::Rect(0, 0, 100, 100));
  ASSERT_EQ(1, docked_strip->num_panels());
  ASSERT_EQ(0, detached_strip->num_panels());

  gfx::Rect panel_old_bounds = panel->GetBounds();

  // Press on title-bar.
  scoped_ptr<NativePanelTesting> panel_testing(
      NativePanelTesting::Create(panel->native_panel()));
  gfx::Point mouse_location(panel->GetBounds().origin());
  panel_testing->PressLeftMouseButtonTitlebar(mouse_location);

  // Drag up the panel to trigger the detach.
  // Expect that the panel is previewed as detached.
  gfx::Point drag_delta_to_detach = GetDragDeltaToDetach();
  mouse_location = mouse_location.Add(drag_delta_to_detach);
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, docked_strip->num_panels());
  ASSERT_EQ(1, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DETACHED, panel->panel_strip()->type());
  gfx::Rect panel_new_bounds = panel_old_bounds;
  panel_new_bounds.Offset(drag_delta_to_detach);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Continue dragging down the panel to trigger the re-attach.
  gfx::Point drag_delta_to_reattach = GetDragDeltaToAttach(panel);
  mouse_location = mouse_location.Add(drag_delta_to_reattach);
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(1, docked_strip->num_panels());
  ASSERT_EQ(0, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DOCKED, panel->panel_strip()->type());
  panel_new_bounds.Offset(drag_delta_to_reattach);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Continue dragging up the panel to trigger the detach again.
  gfx::Point drag_delta_to_detach_again = GetDragDeltaToDetach();
  mouse_location = mouse_location.Add(drag_delta_to_detach_again);
  panel_testing->DragTitlebar(mouse_location);
  ASSERT_EQ(0, docked_strip->num_panels());
  ASSERT_EQ(1, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DETACHED, panel->panel_strip()->type());
  panel_new_bounds.Offset(drag_delta_to_detach_again);
  EXPECT_EQ(panel_new_bounds, panel->GetBounds());

  // Cancel the drag.
  // Expect that the panel stays as docked.
  panel_testing->CancelDragTitlebar();
  ASSERT_EQ(1, docked_strip->num_panels());
  ASSERT_EQ(0, detached_strip->num_panels());
  EXPECT_EQ(PanelStrip::DOCKED, panel->panel_strip()->type());
  EXPECT_EQ(panel_old_bounds, panel->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, DetachWithOverflow) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();
  DetachedPanelStrip* detached_strip = panel_manager->detached_strip();
  OverflowPanelStrip* overflow_strip = panel_manager->overflow_strip();

  gfx::Point drag_delta_to_detach = GetDragDeltaToDetach();

  // Create some docked and overflow panels.
  //   docked:    P1  P2  P3
  //   overflow:  P4  P5
  Panel* panel1 = CreateDockedPanel("1", gfx::Rect(0, 0, 200, 100));
  Panel* panel2 = CreateDockedPanel("2", gfx::Rect(0, 0, 200, 100));
  Panel* panel3 = CreateDockedPanel("3", gfx::Rect(0, 0, 200, 100));
  Panel* panel4 = CreateOverflowPanel("4", gfx::Rect(0, 0, 200, 100));
  Panel* panel5 = CreateOverflowPanel("5", gfx::Rect(0, 0, 200, 100));
  ASSERT_EQ(0, detached_strip->num_panels());
  ASSERT_EQ(3, docked_strip->num_panels());
  ASSERT_EQ(2, overflow_strip->num_panels());

  gfx::Point docked_position1 = panel1->GetBounds().origin();
  gfx::Point docked_position2 = panel2->GetBounds().origin();
  gfx::Point docked_position3 = panel3->GetBounds().origin();

  // Drag to detach the middle docked panel.
  // Expect to have:
  //   detached:  P2
  //   docked:    P1  P3  P4
  //   overflow:  P5
  DragPanelByDelta(panel2, drag_delta_to_detach);
  ASSERT_EQ(1, detached_strip->num_panels());
  ASSERT_EQ(3, docked_strip->num_panels());
  ASSERT_EQ(1, overflow_strip->num_panels());
  EXPECT_EQ(PanelStrip::DOCKED, panel1->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DETACHED, panel2->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DOCKED, panel3->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DOCKED, panel4->panel_strip()->type());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel5->panel_strip()->type());
  EXPECT_EQ(docked_position1, panel1->GetBounds().origin());
  gfx::Point panel2_new_position = docked_position2.Add(drag_delta_to_detach);
  EXPECT_EQ(panel2_new_position, panel2->GetBounds().origin());
  EXPECT_EQ(docked_position2, panel3->GetBounds().origin());
  EXPECT_EQ(docked_position3, panel4->GetBounds().origin());

  // Drag to detach the left-most docked panel.
  // Expect to have:
  //   detached:  P2  P4
  //   docked:    P1  P3  P5
  DragPanelByDelta(panel4, drag_delta_to_detach);
  ASSERT_EQ(2, detached_strip->num_panels());
  ASSERT_EQ(3, docked_strip->num_panels());
  ASSERT_EQ(0, overflow_strip->num_panels());
  EXPECT_EQ(PanelStrip::DOCKED, panel1->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DETACHED, panel2->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DOCKED, panel3->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DETACHED, panel4->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DOCKED, panel5->panel_strip()->type());
  EXPECT_EQ(docked_position1, panel1->GetBounds().origin());
  EXPECT_EQ(panel2_new_position, panel2->GetBounds().origin());
  EXPECT_EQ(docked_position2, panel3->GetBounds().origin());
  gfx::Point panel4_new_position = docked_position3.Add(drag_delta_to_detach);
  EXPECT_EQ(panel4_new_position, panel4->GetBounds().origin());
  EXPECT_EQ(docked_position3, panel5->GetBounds().origin());

  // Drag to detach the right-most docked panel.
  // Expect to have:
  //   detached:  P1  P2  P4
  //   docked:    P3  P5
  DragPanelByDelta(panel1, drag_delta_to_detach);
  ASSERT_EQ(3, detached_strip->num_panels());
  ASSERT_EQ(2, docked_strip->num_panels());
  ASSERT_EQ(0, overflow_strip->num_panels());
  EXPECT_EQ(PanelStrip::DETACHED, panel1->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DETACHED, panel2->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DOCKED, panel3->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DETACHED, panel4->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DOCKED, panel5->panel_strip()->type());
  gfx::Point panel1_new_position = docked_position1.Add(drag_delta_to_detach);
  EXPECT_EQ(panel1_new_position, panel1->GetBounds().origin());
  EXPECT_EQ(panel2_new_position, panel2->GetBounds().origin());
  EXPECT_EQ(docked_position1, panel3->GetBounds().origin());
  EXPECT_EQ(panel4_new_position, panel4->GetBounds().origin());
  EXPECT_EQ(docked_position2, panel5->GetBounds().origin());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelDragBrowserTest, AttachWithOverflow) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  DockedPanelStrip* docked_strip = panel_manager->docked_strip();
  DetachedPanelStrip* detached_strip = panel_manager->detached_strip();
  OverflowPanelStrip* overflow_strip = panel_manager->overflow_strip();

  // Create some detached, docked and overflow panels.
  //   detached:  P1  P2  P3
  //   docked:    P4  P5  P6
  //   overflow:  P7
  Panel* panel1 = CreateDetachedPanel("1", gfx::Rect(100, 300, 200, 100));
  Panel* panel2 = CreateDetachedPanel("2", gfx::Rect(200, 300, 200, 100));
  Panel* panel3 = CreateDetachedPanel("3", gfx::Rect(400, 300, 200, 100));
  Panel* panel4 = CreateDockedPanel("4", gfx::Rect(0, 0, 200, 100));
  Panel* panel5 = CreateDockedPanel("5", gfx::Rect(0, 0, 200, 100));
  Panel* panel6 = CreateDockedPanel("6", gfx::Rect(0, 0, 200, 100));
  Panel* panel7 = CreateOverflowPanel("7", gfx::Rect(0, 0, 200, 100));
  ASSERT_EQ(3, detached_strip->num_panels());
  ASSERT_EQ(3, docked_strip->num_panels());
  ASSERT_EQ(1, overflow_strip->num_panels());

  gfx::Point detached_position1 = panel1->GetBounds().origin();
  gfx::Point detached_position2 = panel2->GetBounds().origin();
  gfx::Point detached_position3 = panel3->GetBounds().origin();
  gfx::Point docked_position1 = panel4->GetBounds().origin();
  gfx::Point docked_position2 = panel5->GetBounds().origin();
  gfx::Point docked_position3 = panel6->GetBounds().origin();

  // Drag to attach a detached panel between 2 docked panels.
  // Expect to have:
  //   detached:  P1  P2
  //   docked:    P4  P3  P5
  //   overflow:  P6  P7
  gfx::Point drag_to_location(panel5->GetBounds().x() + 10,
                              panel5->GetBounds().bottom());
  DragPanelToMouseLocation(panel3, drag_to_location);
  ASSERT_EQ(2, detached_strip->num_panels());
  ASSERT_EQ(3, docked_strip->num_panels());
  ASSERT_EQ(2, overflow_strip->num_panels());
  EXPECT_EQ(PanelStrip::DETACHED, panel1->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DETACHED, panel2->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DOCKED, panel3->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DOCKED, panel4->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DOCKED, panel5->panel_strip()->type());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel6->panel_strip()->type());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel7->panel_strip()->type());
  EXPECT_EQ(detached_position1, panel1->GetBounds().origin());
  EXPECT_EQ(detached_position2, panel2->GetBounds().origin());
  EXPECT_EQ(docked_position2, panel3->GetBounds().origin());
  EXPECT_EQ(docked_position1, panel4->GetBounds().origin());
  EXPECT_EQ(docked_position3, panel5->GetBounds().origin());

  // Drag to attach a detached panel to most-right.
  // Expect to have:
  //   detached:  P1
  //   docked:    P2  P4  P3
  //   overflow:  P5  P6  P7
  gfx::Point drag_to_location2(panel4->GetBounds().right() + 10,
                               panel4->GetBounds().bottom());
  DragPanelToMouseLocation(panel2, drag_to_location2);
  ASSERT_EQ(1, detached_strip->num_panels());
  ASSERT_EQ(3, docked_strip->num_panels());
  ASSERT_EQ(3, overflow_strip->num_panels());
  EXPECT_EQ(PanelStrip::DETACHED, panel1->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DOCKED, panel2->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DOCKED, panel3->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DOCKED, panel4->panel_strip()->type());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel5->panel_strip()->type());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel6->panel_strip()->type());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel7->panel_strip()->type());
  EXPECT_EQ(detached_position1, panel1->GetBounds().origin());
  EXPECT_EQ(docked_position1, panel2->GetBounds().origin());
  EXPECT_EQ(docked_position3, panel3->GetBounds().origin());
  EXPECT_EQ(docked_position2, panel4->GetBounds().origin());

  // Drag to attach a detached panel to most-left.
  // Expect to have:
  //   docked:    P2  P4  P1
  //   overflow:  P3  P5  P6  P7
  gfx::Point drag_to_location3(panel3->GetBounds().x() - 10,
                               panel3->GetBounds().bottom());
  DragPanelToMouseLocation(panel1, drag_to_location3);
  ASSERT_EQ(0, detached_strip->num_panels());
  ASSERT_EQ(3, docked_strip->num_panels());
  ASSERT_EQ(4, overflow_strip->num_panels());
  EXPECT_EQ(PanelStrip::DOCKED, panel1->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DOCKED, panel2->panel_strip()->type());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel3->panel_strip()->type());
  EXPECT_EQ(PanelStrip::DOCKED, panel4->panel_strip()->type());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel5->panel_strip()->type());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel6->panel_strip()->type());
  EXPECT_EQ(PanelStrip::IN_OVERFLOW, panel7->panel_strip()->type());
  EXPECT_EQ(docked_position3, panel1->GetBounds().origin());
  EXPECT_EQ(docked_position1, panel2->GetBounds().origin());
  EXPECT_EQ(docked_position2, panel4->GetBounds().origin());

  panel_manager->CloseAll();
}
