// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/base_panel_browser_test.h"
#include "chrome/browser/ui/panels/detached_panel_strip.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_resize_controller.h"

class PanelResizeBrowserTest : public BasePanelBrowserTest {
 public:
  PanelResizeBrowserTest() : BasePanelBrowserTest() {
  }

  virtual ~PanelResizeBrowserTest() {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    BasePanelBrowserTest::SetUpOnMainThread();

    // All the tests here assume 800x600 work area. Do the check now.
    gfx::Rect display_area = PanelManager::GetInstance()->
        display_settings_provider()->GetDisplayArea();
    DCHECK(display_area.width() == 800);
    DCHECK(display_area.height() == 600);
  }
};

IN_PROC_BROWSER_TEST_F(PanelResizeBrowserTest, DockedPanelsAreNotResizable) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  Panel* panel = CreatePanel("Panel");

  EXPECT_FALSE(panel->CanResizeByMouse());

  gfx::Rect bounds = panel->GetBounds();

  // Try resizing by the top left corner; verify resize won't work.
  gfx::Point mouse_location = bounds.origin();
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_TOP_LEFT);
  mouse_location.Offset(-20, -20);
  panel_manager->ResizeByMouse(mouse_location);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  // Try resizing by the bottom side; verify resize won't work.
  mouse_location = bounds.origin().Add(gfx::Point(10, bounds.height() - 1));
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_BOTTOM);
  mouse_location.Offset(30, -10);
  panel_manager->ResizeByMouse(mouse_location);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  // Try resizing by the bottom right corner; verify resize won't work.
  mouse_location = bounds.origin().Add(
      gfx::Point(bounds.width() - 2, bounds.height()));
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_BOTTOM_RIGHT);
  mouse_location.Offset(20, 10);
  panel_manager->ResizeByMouse(mouse_location);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel->Close();
}

IN_PROC_BROWSER_TEST_F(PanelResizeBrowserTest, ResizeDetachedPanel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  Panel* panel = CreateDetachedPanel("Panel", gfx::Rect(300, 200, 150, 100));

  EXPECT_TRUE(panel->CanResizeByMouse());

  gfx::Rect bounds = panel->GetBounds();

  // Try resizing by the right side; verify resize will change width only.
  gfx::Point mouse_location = bounds.origin().Add(
      gfx::Point(bounds.width() - 1, 30));
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_RIGHT);
  mouse_location.Offset(5, 25);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_width(bounds.width() + 5);
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  // Try resizing by the bottom left side.
  mouse_location = bounds.origin().Add(gfx::Point(1, bounds.height() - 1));
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
  mouse_location = bounds.origin().Add(gfx::Point(bounds.width() - 1, 2));
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
  mouse_location = bounds.origin().Add(gfx::Point(1, 0));
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

IN_PROC_BROWSER_TEST_F(PanelResizeBrowserTest, ResizeDetachedPanelToClampSize) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  Panel* panel = CreateDetachedPanel("Panel", gfx::Rect(300, 200, 150, 100));

  EXPECT_TRUE(panel->CanResizeByMouse());

  gfx::Rect bounds = panel->GetBounds();

  // Make sure the panel does not resize smaller than its min size.
  gfx::Point mouse_location = bounds.origin().Add(
      gfx::Point(30, bounds.height() - 2));
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_BOTTOM);
  mouse_location.Offset(-20, -500);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_height(panel->min_size().height());
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  // Make sure the panel does not resize larger than its size.
  mouse_location = bounds.origin().Add(
      gfx::Point(bounds.width(), bounds.height() - 2));
  panel_manager->StartResizingByMouse(panel, mouse_location,
                                      panel::RESIZE_BOTTOM_RIGHT);
  mouse_location.Offset(500, 40);
  panel_manager->ResizeByMouse(mouse_location);

  bounds.set_size(gfx::Size(panel->max_size().width(), bounds.height() + 40));
  EXPECT_EQ(bounds, panel->GetBounds());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(bounds, panel->GetBounds());

  PanelManager::GetInstance()->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelResizeBrowserTest, CloseDetachedPanelOnResize) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  PanelResizeController* resize_controller = panel_manager->resize_controller();
  DetachedPanelStrip* detached_strip = panel_manager->detached_strip();

  // Create 3 detached panels.
  Panel* panel1 = CreateDetachedPanel("1", gfx::Rect(100, 200, 100, 100));
  Panel* panel2 = CreateDetachedPanel("2", gfx::Rect(200, 210, 110, 110));
  Panel* panel3 = CreateDetachedPanel("3", gfx::Rect(300, 220, 120, 120));
  ASSERT_EQ(3, detached_strip->num_panels());

  gfx::Rect panel1_bounds = panel1->GetBounds();
  gfx::Rect panel2_bounds = panel2->GetBounds();
  gfx::Rect panel3_bounds = panel3->GetBounds();

  // Start resizing panel1, and close panel2 in the process.
  // Panel1 is not affected.
  gfx::Point mouse_location = panel1_bounds.origin().Add(
      gfx::Point(1, panel1_bounds.height() - 1));
  panel_manager->StartResizingByMouse(panel1, mouse_location,
                                      panel::RESIZE_BOTTOM_LEFT);
  mouse_location.Offset(-10, 15);
  panel_manager->ResizeByMouse(mouse_location);

  panel1_bounds.set_size(gfx::Size(panel1_bounds.width() + 10,
                                   panel1_bounds.height() + 15));
  panel1_bounds.Offset(-10, 0);
  EXPECT_EQ(panel1_bounds, panel1->GetBounds());

  CloseWindowAndWait(panel2->browser());
  EXPECT_TRUE(resize_controller->IsResizing());
  EXPECT_EQ(2, detached_strip->num_panels());

  panel_manager->EndResizingByMouse(false);
  EXPECT_EQ(panel1_bounds, panel1->GetBounds());

  // Start resizing panel3, and close it in the process.
  // Resize should abort, panel1 will not be affected.
  mouse_location = panel3_bounds.origin().Add(
      gfx::Point(panel3_bounds.width() - 1, panel3_bounds.height() - 2));
  panel_manager->StartResizingByMouse(panel3, mouse_location,
                                      panel::RESIZE_BOTTOM_RIGHT);
  mouse_location.Offset(7, -12);
  panel_manager->ResizeByMouse(mouse_location);

  panel3_bounds.set_size(gfx::Size(panel3_bounds.width() + 7,
                                   panel3_bounds.height() - 12));
  EXPECT_EQ(panel3_bounds, panel3->GetBounds());

  CloseWindowAndWait(panel3->browser());
  EXPECT_EQ(1, detached_strip->num_panels());
  // Since we closed the panel we were resizing, we should be out of the
  // resizing mode by now.
  EXPECT_FALSE(resize_controller->IsResizing());

  panel_manager->EndResizingByMouse(false);
  EXPECT_FALSE(resize_controller->IsResizing());
  EXPECT_EQ(panel1_bounds, panel1->GetBounds());

  panel_manager->CloseAll();
}

IN_PROC_BROWSER_TEST_F(PanelResizeBrowserTest, ResizeAndCancel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  Panel* panel = CreateDetachedPanel("Panel", gfx::Rect(300, 200, 150, 100));
  PanelResizeController* resize_controller = panel_manager->resize_controller();

  EXPECT_TRUE(panel->CanResizeByMouse());

  gfx::Rect original_bounds = panel->GetBounds();

  // Resizing the panel, then cancelling should return it to the original state.
  // Try resizing by the top right side.
  gfx::Rect bounds = panel->GetBounds();
  gfx::Point mouse_location = bounds.origin().Add(
      gfx::Point(bounds.width() - 1, 1));
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
  mouse_location = bounds.origin().Add(
      gfx::Point(1, bounds.height() - 1));
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
