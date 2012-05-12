// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_drag_controller_interactive_uitest.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/property_bag.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/screen.h"
#include "ui/ui_controls/ui_controls.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using test::GetCenterInScreenCoordinates;
using test::SetID;
using test::ResetIDs;
using test::IDString;
using test::GetTabStripForBrowser;

// The tests in this file exercise detaching the dragged tab into a standalone
// window (not a Browser).

// Creates a browser with two tabs, drags the second to the first.
IN_PROC_BROWSER_TEST_F(TabDragControllerTest, DragInSameWindow) {
  AddTabAndResetBrowser(browser());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  gfx::Point tab_1_center(GetCenterInScreenCoordinates(tab_strip->tab_at(1)));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_1_center));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_0_center));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::UP));
  EXPECT_EQ("1 0", IDString(model));
  EXPECT_FALSE(TabDragController::IsActive());
  EXPECT_FALSE(tab_strip->IsDragSessionActive());
}

// Creates two browsers, drags from first into second.
IN_PROC_BROWSER_TEST_F(TabDragControllerTest, DragToSeparateWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_0_center));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
                  gfx::Point(tab_0_center.x(),
                             tab_0_center.y() + tab_strip->height() + 20)));
  ASSERT_TRUE(TabDragController::IsActive());

  // Drag into the second browser.
  gfx::Point target_point(tab_strip2->width() -1, tab_strip2->height() / 2);
  views::View::ConvertPointToScreen(tab_strip2, &target_point);
  ASSERT_TRUE(ui_controls::SendMouseMove(target_point.x(), target_point.y()));

  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, ending the drag session.
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::UP));
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
}

// Drags from browser to separate window and releases mouse.
IN_PROC_BROWSER_TEST_F(TabDragControllerTest, DetachToOwnWindow) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_0_center));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));
  ASSERT_TRUE(ui_controls::SendMouseMove(
      tab_0_center.x(), tab_0_center.y() + tab_strip->height() + 20));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::UP));

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, BrowserList::size());
  Browser* new_browser = BrowserList::GetLastActive();
  ASSERT_NE(browser(), new_browser);
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
}

// Deletes a tab being dragged before the user moved enough to start a drag.
IN_PROC_BROWSER_TEST_F(TabDragControllerTest, DeleteBeforeStartedDragging) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Click on the first tab, but don't move it.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_0_center));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));

  // Should be dragging.
  ASSERT_TRUE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Delete the tab being dragged.
  delete browser()->tab_strip_model()->GetTabContentsAt(0);

  // Should have canceled dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
}

// Deletes a tab being dragged while still attached.
IN_PROC_BROWSER_TEST_F(TabDragControllerTest, DeleteTabWhileAttached) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Click on the first tab and move it enough so that it starts dragging but is
  // still attached.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_0_center));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
                  gfx::Point(tab_0_center.x() + 20, tab_0_center.y())));

  // Should be dragging.
  ASSERT_TRUE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Delete the tab being dragged.
  delete browser()->tab_strip_model()->GetTabContentsAt(0);

  // Should have canceled dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
}

// Deletes a tab being dragged after dragging a tab so that a new window is
// created.
IN_PROC_BROWSER_TEST_F(TabDragControllerTest, DeleteTabWhileDetached) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  TabContentsWrapper* to_delete =
      browser()->tab_strip_model()->GetTabContentsAt(0);
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_0_center));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
                  gfx::Point(tab_0_center.x(),
                             tab_0_center.y() + tab_strip->height() + 20)));
  delete to_delete;

  // Should not be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
}

namespace {

void DeleteSourceDetachedStep2(TabContentsWrapper* tab) {
  // This ends up closing the source window.
  delete tab;
  // Cancel the drag.
  ui_controls::SendKeyPress(NULL, ui::VKEY_ESCAPE, false, false, false, false);
}

}  // namespace

// Detaches a tab and while detached deletes a tab from the source and releases
// the mouse.
IN_PROC_BROWSER_TEST_F(TabDragControllerTest, DeleteSourceDetached) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  TabContentsWrapper* to_delete =
      browser()->tab_strip_model()->GetTabContentsAt(1);
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_0_center));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
                  gfx::Point(tab_0_center.x(),
                             tab_0_center.y() + tab_strip->height() + 20)));
  delete to_delete;

  // Should still be dragging.
  ASSERT_TRUE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse.
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::UP));

  // Releasing the mouse should destroy the existing browser and create a new
  // one.
  ASSERT_EQ(1u, BrowserList::size());
  Browser* new_browser = *BrowserList::begin();
  EXPECT_NE(new_browser, browser());

  ASSERT_FALSE(GetTabStripForBrowser(new_browser)->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
}

// Creates two browsers, selects all tabs in first and drags into second.
IN_PROC_BROWSER_TEST_F(TabDragControllerTest, DragAllToSeparateWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  browser()->tab_strip_model()->AddTabAtToSelection(0);
  browser()->tab_strip_model()->AddTabAtToSelection(1);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_0_center));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
                  gfx::Point(tab_0_center.x(),
                             tab_0_center.y() + tab_strip->height() + 20)));

  ASSERT_TRUE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, BrowserList::size());

  // Drag to tab_strip2.
  gfx::Point target_point(tab_strip2->width() - 1,
                          tab_strip2->height() / 2);
  views::View::ConvertPointToScreen(tab_strip2, &target_point);
  ASSERT_TRUE(ui_controls::SendMouseMove(target_point.x(), target_point.y()));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::UP));
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0 1", IDString(browser2->tab_strip_model()));
}

// Creates two browsers, selects all tabs in first, drags into second, then hits
// escape.
IN_PROC_BROWSER_TEST_F(TabDragControllerTest,
                       DragAllToSeparateWindowAndCancel) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  browser()->tab_strip_model()->AddTabAtToSelection(0);
  browser()->tab_strip_model()->AddTabAtToSelection(1);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_0_center));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
                  gfx::Point(tab_0_center.x(),
                             tab_0_center.y() + tab_strip->height() + 20)));
  ASSERT_TRUE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, BrowserList::size());

  // Drag to tab_strip2.
  gfx::Point target_point(tab_strip2->width() - 1,
                          tab_strip2->height() / 2);
  views::View::ConvertPointToScreen(tab_strip2, &target_point);
  ASSERT_TRUE(ui_controls::SendMouseMove(target_point.x(), target_point.y()));

  ASSERT_TRUE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, BrowserList::size());

  // Cancel the drag.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser2, ui::VKEY_ESCAPE, false, false, false, false));

  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  ASSERT_EQ(2u, BrowserList::size());
  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));
  EXPECT_EQ("100", IDString(browser2->tab_strip_model()));
}
