// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/detached_panel_collection.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_resize_controller.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"

class PanelResizeBrowserTest : public BasePanelBrowserTest {
 public:
  PanelResizeBrowserTest() : BasePanelBrowserTest() {
  }

  virtual ~PanelResizeBrowserTest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    BasePanelBrowserTest::SetUpOnMainThread();

    // All the tests here assume using mocked 800x600 display area for the
    // primary monitor. Do the check now.
    gfx::Rect primary_display_area = PanelManager::GetInstance()->
        display_settings_provider()->GetPrimaryDisplayArea();
    DCHECK(primary_display_area.width() == 800);
    DCHECK(primary_display_area.height() == 600);
  }

  void ResizePanel(Panel* panel,
                   panel::ResizingSides sides,
                   const gfx::Vector2d& delta) {
    PanelManager* panel_manager = PanelManager::GetInstance();
    gfx::Rect bounds = panel->GetBounds();
    gfx::Point mouse_location;
    switch (sides) {
      case panel::RESIZE_TOP_LEFT:
        mouse_location = bounds.origin();
        break;
      case panel::RESIZE_TOP:
        mouse_location.SetPoint(bounds.x() + bounds.width() / 2, bounds.y());
        break;
      case panel::RESIZE_TOP_RIGHT:
        mouse_location.SetPoint(bounds.right(), bounds.y());
        break;
      case panel::RESIZE_LEFT:
        mouse_location.SetPoint(bounds.x(), bounds.y() + bounds.height() / 2);
        break;
      case panel::RESIZE_RIGHT:
        mouse_location.SetPoint(bounds.right(),
                                bounds.y() + bounds.height() / 2);
        break;
      case panel::RESIZE_BOTTOM_LEFT:
        mouse_location.SetPoint(bounds.x(), bounds.bottom());
        break;
      case panel::RESIZE_BOTTOM:
        mouse_location.SetPoint(bounds.x() + bounds.width() / 2,
                                bounds.bottom());
        break;
      case panel::RESIZE_BOTTOM_RIGHT:
        mouse_location.SetPoint(bounds.right(), bounds.bottom());
        break;
      default:
        NOTREACHED();
        break;
    }
    panel_manager->StartResizingByMouse(panel, mouse_location, sides);
    mouse_location += delta;
    panel_manager->ResizeByMouse(mouse_location);
    panel_manager->EndResizingByMouse(false);
  }
};

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_DockedPanelResizability DISABLED_DockedPanelResizability
#else
#define MAYBE_DockedPanelResizability DockedPanelResizability
#endif
IN_PROC_BROWSER_TEST_F(PanelResizeBrowserTest, MAYBE_DockedPanelResizability) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  Panel* panel = CreatePanel("Panel");

  EXPECT_EQ(panel::RESIZABLE_EXCEPT_BOTTOM, panel->CanResizeByMouse());

  gfx::Rect bounds = panel->GetBounds();

  // Try resizing by the top left corner.
  gfx::Point mouse_location = bounds.origin();
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_TOP_LEFT);
  mouse_location.Offset(-20, -10);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_size(gfx::Size(bounds.width() + 20, bounds.height() + 10));
  bounds.Offset(-20, -10);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  // Try resizing by the top.
  mouse_location = bounds.origin() + gfx::Vector2d(10, 1);
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_TOP);
  mouse_location.Offset(5, -10);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_height(bounds.height() + 10);
  bounds.Offset(0, -10);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  // Try resizing by the left side.
  mouse_location = bounds.origin() + gfx::Vector2d(1, 30);
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_LEFT);
  mouse_location.Offset(-5, 25);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_width(bounds.width() + 5);
  bounds.Offset(-5, 0);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  // Try resizing by the top right side.
  mouse_location = bounds.origin() + gfx::Vector2d(bounds.width() - 1, 2);
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_TOP_RIGHT);
  mouse_location.Offset(30, 20);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_size(gfx::Size(bounds.width() + 30, bounds.height() - 20));
  bounds.Offset(0, 20);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  WaitForBoundsAnimationFinished(panel);
  bounds.Offset(-30, 0);  // Layout of panel adjusted in docked collection.
  EXPECT_EQ(bounds, panel->GetBounds());

  // Try resizing by the right side.
  mouse_location = bounds.origin() + gfx::Vector2d(bounds.width() - 1, 30);
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_RIGHT);
  mouse_location.Offset(5, 25);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_width(bounds.width() + 5);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  WaitForBoundsAnimationFinished(panel);
  bounds.Offset(-5, 0);  // Layout of panel adjusted in docked collection.
  EXPECT_EQ(bounds, panel->GetBounds());

  // Try resizing by the bottom side; verify resize won't work.
  mouse_location = bounds.origin() + gfx::Vector2d(10, bounds.height() - 1);
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_BOTTOM);
  mouse_location.Offset(30, -10);
  panel_manager->ResizeByMouse(mouse_location);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  // Try resizing by the bottom left corner; verify resize won't work.
  mouse_location = bounds.origin() + gfx::Vector2d(1, bounds.height() - 1);
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_BOTTOM_LEFT);
  mouse_location.Offset(-10, 15);
  panel_manager->ResizeByMouse(mouse_location);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  // Try resizing by the bottom right corner; verify resize won't work.
  mouse_location = bounds.origin() +
      gfx::Vector2d(bounds.width() - 2, bounds.height());
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_BOTTOM_RIGHT);
  mouse_location.Offset(20, 10);
  panel_manager->ResizeByMouse(mouse_location);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel->Close();
}

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_ResizeDetachedPanel DISABLED_ResizeDetachedPanel
#else
#define MAYBE_ResizeDetachedPanel ResizeDetachedPanel
#endif
IN_PROC_BROWSER_TEST_F(PanelResizeBrowserTest, MAYBE_ResizeDetachedPanel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  Panel* panel = CreateDetachedPanel("Panel", gfx::Rect(300, 200, 150, 100));

  EXPECT_EQ(panel::RESIZABLE_ALL, panel->CanResizeByMouse());

  gfx::Rect bounds = panel->GetBounds();

  // Try resizing by the right side; verify resize will change width only.
  gfx::Point mouse_location = bounds.origin() +
      gfx::Vector2d(bounds.width() - 1, 30);
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_RIGHT);
  mouse_location.Offset(5, 25);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_width(bounds.width() + 5);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  // Try resizing by the bottom left side.
  mouse_location = bounds.origin() + gfx::Vector2d(1, bounds.height() - 1);
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_BOTTOM_LEFT);
  mouse_location.Offset(-10, 15);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_size(gfx::Size(bounds.width() + 10, bounds.height() + 15));
  bounds.Offset(-10, 0);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  // Try resizing by the top right side.
  mouse_location = bounds.origin() + gfx::Vector2d(bounds.width() - 1, 2);
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_TOP_RIGHT);
  mouse_location.Offset(30, 20);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_size(gfx::Size(bounds.width() + 30, bounds.height() - 20));
  bounds.Offset(0, 20);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  // Try resizing by the top left side.
  mouse_location = bounds.origin() + gfx::Vector2d(1, 0);
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_TOP_LEFT);
  mouse_location.Offset(-20, -10);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_size(gfx::Size(bounds.width() + 20, bounds.height() + 10));
  bounds.Offset(-20, -10);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  PanelManager::GetInstance()->CloseAll();
}

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_TryResizePanelBelowMinimizeSize \
  DISABLED_TryResizePanelBelowMinimizeSize
#else
#define MAYBE_TryResizePanelBelowMinimizeSize TryResizePanelBelowMinimizeSize
#endif
IN_PROC_BROWSER_TEST_F(PanelResizeBrowserTest,
                       MAYBE_TryResizePanelBelowMinimizeSize) {
  int initial_width = 150;
  int initial_height = 100;
  Panel* panel = CreateDetachedPanel("1",
      gfx::Rect(300, 200, initial_width, initial_height));

  // Try to resize the panel below the minimum size. Expect that the panel
  // shrinks to the minimum size.
  int resize_width = panel::kPanelMinWidth / 2 - initial_width;
  int resize_height = panel::kPanelMinHeight / 2 - initial_height;
  ResizePanel(panel,
              panel::RESIZE_BOTTOM_RIGHT,
              gfx::Vector2d(resize_width, resize_height));

  EXPECT_EQ(panel::kPanelMinWidth, panel->GetBounds().width());
  EXPECT_EQ(panel::kPanelMinHeight, panel->GetBounds().height());

  PanelManager::GetInstance()->CloseAll();
}

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_ResizeDetachedPanelToClampSize \
  DISABLED_ResizeDetachedPanelToClampSize
#else
#define MAYBE_ResizeDetachedPanelToClampSize ResizeDetachedPanelToClampSize
#endif
IN_PROC_BROWSER_TEST_F(PanelResizeBrowserTest,
                       MAYBE_ResizeDetachedPanelToClampSize) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  Panel* panel = CreateDetachedPanel("Panel", gfx::Rect(300, 200, 150, 100));

  EXPECT_EQ(panel::RESIZABLE_ALL, panel->CanResizeByMouse());

  gfx::Rect bounds = panel->GetBounds();

  // Make sure the panel does not resize smaller than its min size.
  gfx::Point mouse_location = bounds.origin() +
      gfx::Vector2d(30, bounds.height() - 2);
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_BOTTOM);
  mouse_location.Offset(-20, -500);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_height(panel->min_size().height());
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  // Make sure the panel can resize larger than its size. User is in control.
  mouse_location = bounds.origin() +
      gfx::Vector2d(bounds.width(), bounds.height() - 2);
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_BOTTOM_RIGHT);

  // This drag would take us beyond max size.
  int delta_x = panel->max_size().width() + 10 - panel->GetBounds().width();
  int delta_y = panel->max_size().height() + 10 - panel->GetBounds().height();
  mouse_location.Offset(delta_x, delta_y);
  panel_manager->ResizeByMouse(mouse_location);

  // The bounds if the max_size does not limit the resize.
  bounds.set_size(gfx::Size(bounds.width() + delta_x,
                            bounds.height() + delta_y));
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  PanelManager::GetInstance()->CloseAll();
}

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_CloseDetachedPanelOnResize DISABLED_CloseDetachedPanelOnResize
#else
#define MAYBE_CloseDetachedPanelOnResize CloseDetachedPanelOnResize
#endif
IN_PROC_BROWSER_TEST_F(PanelResizeBrowserTest,
                       MAYBE_CloseDetachedPanelOnResize) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  PanelResizeController* resize_controller = panel_manager->resize_controller();
  DetachedPanelCollection* detached_collection =
      panel_manager->detached_collection();

  // Create 3 detached panels.
  Panel* panel1 = CreateDetachedPanel("1", gfx::Rect(100, 200, 100, 100));
  Panel* panel2 = CreateDetachedPanel("2", gfx::Rect(200, 210, 110, 110));
  Panel* panel3 = CreateDetachedPanel("3", gfx::Rect(300, 220, 120, 120));
  ASSERT_EQ(3, detached_collection->num_panels());

  gfx::Rect panel1_bounds = panel1->GetBounds();
  gfx::Rect panel2_bounds = panel2->GetBounds();
  gfx::Rect panel3_bounds = panel3->GetBounds();

  // Start resizing panel1, and close panel2 in the process.
  // Panel1 is not affected.
  gfx::Point mouse_location = panel1_bounds.origin() +
      gfx::Vector2d(1, panel1_bounds.height() - 1);
  panel_manager->StartResizingByMouse(panel1, mouse_location,
                                      panel::RESIZE_BOTTOM_LEFT);
  mouse_location.Offset(-10, 15);
  panel_manager->ResizeByMouse(mouse_location);

  panel1_bounds.set_size(gfx::Size(panel1_bounds.width() + 10,
                                   panel1_bounds.height() + 15));
  panel1_bounds.Offset(-10, 0);
  EXPECT_EQ(panel1_bounds, panel1->GetBounds());

  CloseWindowAndWait(panel2);
  EXPECT_TRUE(resize_controller->IsResizing());
  EXPECT_EQ(2, detached_collection->num_panels());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(panel1_bounds, panel1->GetBounds());

  // Start resizing panel3, and close it in the process.
  // Resize should abort, panel1 will not be affected.
  mouse_location = panel3_bounds.origin() +
      gfx::Vector2d(panel3_bounds.width() - 1, panel3_bounds.height() - 2);
  panel_manager->StartResizingByMouse(panel3, mouse_location,
                                      panel::RESIZE_BOTTOM_RIGHT);
  mouse_location.Offset(7, -12);
  panel_manager->ResizeByMouse(mouse_location);

  panel3_bounds.set_size(gfx::Size(panel3_bounds.width() + 7,
                                   panel3_bounds.height() - 12));
  EXPECT_EQ(panel3_bounds, panel3->GetBounds());

  CloseWindowAndWait(panel3);
  EXPECT_EQ(1, detached_collection->num_panels());
  // Since we closed the panel we were resizing, we should be out of the
  // resizing mode by now.
  EXPECT_FALSE(resize_controller->IsResizing());

  panel_manager->EndResizingByMouse(false);
  EXPECT_FALSE(resize_controller->IsResizing());
  EXPECT_EQ(panel1_bounds, panel1->GetBounds());

  panel_manager->CloseAll();
}

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_ResizeAndCancel DISABLED_ResizeAndCancel
#else
#define MAYBE_ResizeAndCancel ResizeAndCancel
#endif
IN_PROC_BROWSER_TEST_F(PanelResizeBrowserTest, MAYBE_ResizeAndCancel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  Panel* panel = CreateDetachedPanel("Panel", gfx::Rect(300, 200, 150, 100));
  PanelResizeController* resize_controller = panel_manager->resize_controller();

  EXPECT_EQ(panel::RESIZABLE_ALL, panel->CanResizeByMouse());

  gfx::Rect original_bounds = panel->GetBounds();

  // Resizing the panel, then cancelling should return it to the original state.
  // Try resizing by the top right side.
  gfx::Rect bounds = panel->GetBounds();
  gfx::Point mouse_location = bounds.origin() +
      gfx::Vector2d(bounds.width() - 1, 1);
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_TOP_RIGHT);
  mouse_location.Offset(5, 25);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_size(gfx::Size(bounds.width() + 5, bounds.height() - 25));
  bounds.Offset(0, 25);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(true);
  EXPECT_EQ(original_bounds, panel->GetBounds());

  // Try resizing by the bottom left side.
  bounds = panel->GetBounds();
  mouse_location = bounds.origin() + gfx::Vector2d(1, bounds.height() - 1);
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_BOTTOM_LEFT);
  mouse_location.Offset(-10, 15);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_size(gfx::Size(bounds.width() + 10, bounds.height() + 15));
  bounds.Offset(-10, 0);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(true);
  EXPECT_EQ(original_bounds, panel->GetBounds());
  EXPECT_FALSE(resize_controller->IsResizing());

  panel_manager->CloseAll();
}

// http://crbug.com/175760; several panel tests failing regularly on mac.
#if defined(OS_MACOSX)
#define MAYBE_ResizeDetachedPanelToTop DISABLED_ResizeDetachedPanelToTop
#else
#define MAYBE_ResizeDetachedPanelToTop ResizeDetachedPanelToTop
#endif
IN_PROC_BROWSER_TEST_F(PanelResizeBrowserTest, MAYBE_ResizeDetachedPanelToTop) {
  // Setup the test areas to have top-aligned bar excluded from work area.
  const gfx::Rect primary_display_area(0, 0, 800, 600);
  const gfx::Rect primary_work_area(0, 10, 800, 590);
  mock_display_settings_provider()->SetPrimaryDisplay(
      primary_display_area, primary_work_area);

  PanelManager* panel_manager = PanelManager::GetInstance();
  Panel* panel = CreateDetachedPanel("1", gfx::Rect(300, 200, 250, 200));
  gfx::Rect bounds = panel->GetBounds();

  // Try resizing by the top left corner.
  gfx::Point mouse_location = bounds.origin();
  panel_manager->StartResizingByMouse(panel,
                                      mouse_location,
                                      panel::RESIZE_TOP_LEFT);

  // Try moving the mouse outside the top of the work area. Expect that panel's
  // top position will not exceed the top of the work area.
  mouse_location = gfx::Point(250, 2);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_width(bounds.width() + bounds.x() - mouse_location.x());
  bounds.set_height(bounds.height() + bounds.y() - primary_work_area.y());
  bounds.set_x(mouse_location.x());
  bounds.set_y(primary_work_area.y());
  EXPECT_EQ(bounds, panel->GetBounds());

  // Try moving the mouse inside the work area. Expect that the panel can be
  // resized without constraint.
  mouse_location = gfx::Point(280, 50);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_width(bounds.width() + bounds.x() - mouse_location.x());
  bounds.set_height(bounds.height() + bounds.y() - mouse_location.y());
  bounds.set_x(mouse_location.x());
  bounds.set_y(mouse_location.y());
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->CloseAll();
}

// TODO(jianli): to be enabled for other platforms when stacked panels are
// supported.
#if defined(OS_WIN)

IN_PROC_BROWSER_TEST_F(PanelResizeBrowserTest, ResizeStackedPanels) {
  PanelManager* panel_manager = PanelManager::GetInstance();

  // Create 3 stacked panels.
  StackedPanelCollection* stack = panel_manager->CreateStack();
  gfx::Rect panel1_initial_bounds = gfx::Rect(100, 50, 200, 150);
  Panel* panel1 = CreateStackedPanel("1", panel1_initial_bounds, stack);
  gfx::Rect panel2_initial_bounds = gfx::Rect(0, 0, 150, 100);
  Panel* panel2 = CreateStackedPanel("2", panel2_initial_bounds, stack);
  gfx::Rect panel3_initial_bounds = gfx::Rect(0, 0, 120, 110);
  Panel* panel3 = CreateStackedPanel("3", panel3_initial_bounds, stack);
  ASSERT_EQ(3, panel_manager->num_panels());
  ASSERT_EQ(1, panel_manager->num_stacks());
  ASSERT_EQ(3, stack->num_panels());

  gfx::Size panel1_expected_full_size = panel1_initial_bounds.size();
  EXPECT_EQ(panel1_expected_full_size, panel1->full_size());
  gfx::Size panel2_expected_full_size(panel1_initial_bounds.width(),
                                      panel2_initial_bounds.height());
  EXPECT_EQ(panel2_expected_full_size, panel2->full_size());
  gfx::Size panel3_expected_full_size(panel1_initial_bounds.width(),
                                      panel3_initial_bounds.height());
  EXPECT_EQ(panel3_expected_full_size, panel3->full_size());

  gfx::Rect panel1_expected_bounds(panel1_initial_bounds);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  gfx::Rect panel2_expected_bounds(panel1_expected_bounds.x(),
                                   panel1_expected_bounds.bottom(),
                                   panel1_expected_bounds.width(),
                                   panel2_initial_bounds.height());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  gfx::Rect panel3_expected_bounds(panel2_expected_bounds.x(),
                                   panel2_expected_bounds.bottom(),
                                   panel2_expected_bounds.width(),
                                   panel3_initial_bounds.height());
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  // Resize by the top-left corner of the top panel.
  // Expect that the width of all stacked panels get increased by the same
  // amount and the top panel also expands in height.
  int top_resize_width = 15;
  int top_resize_height = 10;
  ResizePanel(panel1,
              panel::RESIZE_TOP_LEFT,
              gfx::Vector2d(-top_resize_width, -top_resize_height));

  panel1_expected_full_size.Enlarge(top_resize_width, top_resize_height);
  EXPECT_EQ(panel1_expected_full_size, panel1->full_size());
  panel2_expected_full_size.Enlarge(top_resize_width, 0);
  EXPECT_EQ(panel2_expected_full_size, panel2->full_size());
  panel3_expected_full_size.Enlarge(top_resize_width, 0);
  EXPECT_EQ(panel3_expected_full_size, panel3->full_size());

  panel1_expected_bounds.SetRect(
      panel1_expected_bounds.x() - top_resize_width,
      panel1_expected_bounds.y() - top_resize_height,
      panel1_expected_bounds.width() + top_resize_width,
      panel1_expected_bounds.height() + top_resize_height);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.set_x(panel2_expected_bounds.x() - top_resize_width);
  panel2_expected_bounds.set_width(
      panel2_expected_bounds.width() + top_resize_width);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel3_expected_bounds.set_x(panel3_expected_bounds.x() - top_resize_width);
  panel3_expected_bounds.set_width(
      panel3_expected_bounds.width() + top_resize_width);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  // Resize by the bottom-right corner of the bottom panel.
  // Expect that the width of all stacked panels get increased by the same
  // amount and the bottom panel also shrinks in height.
  int bottom_resize_width = 12;
  int bottom_resize_height = 8;
  ResizePanel(panel3,
              panel::RESIZE_BOTTOM_RIGHT,
              gfx::Vector2d(-bottom_resize_width, -bottom_resize_height));

  panel1_expected_full_size.Enlarge(-bottom_resize_width, 0);
  EXPECT_EQ(panel1_expected_full_size, panel1->full_size());
  panel2_expected_full_size.Enlarge(-bottom_resize_width, 0);
  EXPECT_EQ(panel2_expected_full_size, panel2->full_size());
  panel3_expected_full_size.Enlarge(-bottom_resize_width,
                                    -bottom_resize_height);
  EXPECT_EQ(panel3_expected_full_size, panel3->full_size());

  panel1_expected_bounds.set_width(
      panel1_expected_bounds.width() - bottom_resize_width);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.set_width(
      panel2_expected_bounds.width() - bottom_resize_width);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel3_expected_bounds.set_width(
      panel3_expected_bounds.width() - bottom_resize_width);
  panel3_expected_bounds.set_height(
      panel3_expected_bounds.height() - bottom_resize_height);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  // Resize by the bottom edge of the middle panel.
  // Expect that the height of the middle panel increases and the height of
  // the bottom panel decreases by the same amount.
  int middle_resize_height = 5;
  ResizePanel(panel2,
              panel::RESIZE_BOTTOM,
              gfx::Vector2d(0, middle_resize_height));

  EXPECT_EQ(panel1_expected_full_size, panel1->full_size());
  panel2_expected_full_size.Enlarge(0, middle_resize_height);
  EXPECT_EQ(panel2_expected_full_size, panel2->full_size());
  panel3_expected_full_size.Enlarge(0, -middle_resize_height);
  EXPECT_EQ(panel3_expected_full_size, panel3->full_size());

  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.set_height(
      panel2_expected_bounds.height() + middle_resize_height);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel3_expected_bounds.set_y(
      panel3_expected_bounds.y() + middle_resize_height);
  panel3_expected_bounds.set_height(
      panel3_expected_bounds.height() - middle_resize_height);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  // Collapse the middle panel.
  panel2->Minimize();
  WaitForBoundsAnimationFinished(panel2);
  EXPECT_TRUE(panel2->IsMinimized());

  EXPECT_EQ(panel1_expected_full_size, panel1->full_size());
  EXPECT_EQ(panel2_expected_full_size, panel2->full_size());
  EXPECT_EQ(panel3_expected_full_size, panel3->full_size());

  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.set_height(panel2->TitleOnlyHeight());
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel3_expected_bounds.set_y(panel2_expected_bounds.bottom());
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  // Resize by the bottom edge of the top panel.
  // Expect that the height of the top panel increases and the height of
  // the middle panel is not affected because it is collapsed.
  top_resize_height = 18;
  ResizePanel(panel1,
              panel::RESIZE_BOTTOM,
              gfx::Vector2d(0, top_resize_height));

  panel1_expected_full_size.Enlarge(0, top_resize_height);
  EXPECT_EQ(panel1_expected_full_size, panel1->full_size());
  EXPECT_EQ(panel2_expected_full_size, panel2->full_size());
  EXPECT_EQ(panel3_expected_full_size, panel3->full_size());

  panel1_expected_bounds.set_height(
      panel1_expected_bounds.height() + top_resize_height);
  EXPECT_EQ(panel1_expected_bounds, panel1->GetBounds());
  panel2_expected_bounds.set_y(
      panel2_expected_bounds.y() + top_resize_height);
  EXPECT_EQ(panel2_expected_bounds, panel2->GetBounds());
  panel3_expected_bounds.set_y(
      panel3_expected_bounds.y() + top_resize_height);
  EXPECT_EQ(panel3_expected_bounds, panel3->GetBounds());

  panel_manager->CloseAll();
}

#endif
