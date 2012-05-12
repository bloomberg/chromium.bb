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

namespace test {

class QuitDraggingObserver : public content::NotificationObserver {
 public:
  QuitDraggingObserver() {
    registrar_.Add(this, chrome::NOTIFICATION_TAB_DRAG_LOOP_DONE,
                   content::NotificationService::AllSources());
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(chrome::NOTIFICATION_TAB_DRAG_LOOP_DONE, type);
    MessageLoopForUI::current()->Quit();
    delete this;
  }

 private:
  virtual ~QuitDraggingObserver() {}

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(QuitDraggingObserver);
};

gfx::Point GetCenterInScreenCoordinates(const views::View* view) {
  gfx::Point center(view->width() / 2, view->height() / 2);
  views::View::ConvertPointToScreen(view, &center);
  return center;
}

base::PropertyAccessor<int>* id_accessor() {
  static base::PropertyAccessor<int>* accessor = NULL;
  if (!accessor)
    accessor = new base::PropertyAccessor<int>;
  return accessor;
}

void SetID(content::WebContents* tab_contents, int id) {
  id_accessor()->SetProperty(tab_contents->GetPropertyBag(), id);
}

void ResetIDs(TabStripModel* model, int start) {
  for (int i = 0; i < model->count(); ++i)
    SetID(model->GetTabContentsAt(i)->web_contents(), start + i);
}

std::string IDString(TabStripModel* model) {
  std::string result;
  for (int i = 0; i < model->count(); ++i) {
    if (i != 0)
      result += " ";
    int* id_value = id_accessor()->GetProperty(
        model->GetTabContentsAt(i)->web_contents()->GetPropertyBag());
    if (id_value)
      result += base::IntToString(*id_value);
    else
      result += "?";
  }
  return result;
}

// Creates a listener that quits the message loop when no longer dragging.
void QuitWhenNotDragging() {
  new QuitDraggingObserver();  // QuitDraggingObserver deletes itself.
}

TabStrip* GetTabStripForBrowser(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  return static_cast<TabStrip*>(browser_view->tabstrip());
}

}  // namespace test

using test::GetCenterInScreenCoordinates;
using test::SetID;
using test::ResetIDs;
using test::IDString;
using test::QuitWhenNotDragging;
using test::GetTabStripForBrowser;

TabDragControllerTest::TabDragControllerTest() {
}

TabDragControllerTest::~TabDragControllerTest() {
}

void TabDragControllerTest::StopAnimating(TabStrip* tab_strip) {
  tab_strip->StopAnimating(true);
}

void TabDragControllerTest::AddTabAndResetBrowser(Browser* browser) {
  AddBlankTabAndShow(browser);
  StopAnimating(GetTabStripForBrowser(browser));
  ResetIDs(browser->tab_strip_model(), 0);
}

Browser* TabDragControllerTest::CreateAnotherWindowBrowserAndRelayout() {
  // Add another tab.
  AddTabAndResetBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  ResetIDs(browser2->tab_strip_model(), 100);

  // Resize the two windows so they're right next to each other.
  gfx::Rect work_area = gfx::Screen::GetMonitorNearestWindow(
      browser()->window()->GetNativeHandle()).work_area();
  gfx::Size half_size =
      gfx::Size(work_area.width() / 3 - 10, work_area.height() / 2 - 10);
  browser()->window()->SetBounds(gfx::Rect(work_area.origin(), half_size));
  browser2->window()->SetBounds(gfx::Rect(
      work_area.x() + half_size.width(), work_area.y(),
      half_size.width(), half_size.height()));
  return browser2;
}

class DetachToBrowserTabDragControllerTest : public TabDragControllerTest {
 public:
  DetachToBrowserTabDragControllerTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kTabBrowserDragging);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DetachToBrowserTabDragControllerTest);
};

// Creates a browser with two tabs, drags the second to the first.
IN_PROC_BROWSER_TEST_F(DetachToBrowserTabDragControllerTest, DragInSameWindow) {
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

namespace {

// Invoked from the nested message loop.
void DragToSeparateWindowStep2(TabStrip* not_attached_tab_strip,
                               TabStrip* target_tab_strip) {
  ASSERT_FALSE(not_attached_tab_strip->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  gfx::Point target_point(target_tab_strip->width() -1,
                          target_tab_strip->height() / 2);
  views::View::ConvertPointToScreen(target_tab_strip, &target_point);
  ASSERT_TRUE(ui_controls::SendMouseMove(target_point.x(), target_point.y()));
}

}  // namespace

// Creates two browsers, drags from first into second.
IN_PROC_BROWSER_TEST_F(DetachToBrowserTabDragControllerTest,
                       DragToSeparateWindow) {
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
  ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + tab_strip->height() + 20,
                  base::Bind(&DragToSeparateWindowStep2,
                             tab_strip, tab_strip2)));
  // Schedule observer to quit message loop when done dragging. This has to be
  // async so the message loop can run.
  QuitWhenNotDragging();
  MessageLoop::current()->Run();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::UP));
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
}

// Drags from browser to separate window and releases mouse.
IN_PROC_BROWSER_TEST_F(DetachToBrowserTabDragControllerTest,
                       DetachToOwnWindow) {
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
  ASSERT_TRUE(ui_controls::SendMouseEvents(
                  ui_controls::LEFT, ui_controls::UP));
  // Schedule observer to quit message loop when done dragging. This has to be
  // async so the message loop can run.
  QuitWhenNotDragging();
  MessageLoop::current()->Run();

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
IN_PROC_BROWSER_TEST_F(DetachToBrowserTabDragControllerTest,
                       DeleteBeforeStartedDragging) {
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
IN_PROC_BROWSER_TEST_F(DetachToBrowserTabDragControllerTest,
                       DeleteTabWhileAttached) {
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

namespace {

void DeleteWhileDetachedStep2(TabContentsWrapper* tab) {
  delete tab;
}

}  // namespace

// Deletes a tab being dragged after dragging a tab so that a new window is
// created.
IN_PROC_BROWSER_TEST_F(DetachToBrowserTabDragControllerTest,
                       DeleteTabWhileDetached) {
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
  ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + tab_strip->height() + 20,
      base::Bind(&DeleteWhileDetachedStep2, to_delete)));
  // Schedule observer to quit message loop when done dragging. This has to be
  // async so the message loop can run.
  QuitWhenNotDragging();
  MessageLoop::current()->Run();

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

// Detaches a tab and while detached deletes a tab from the source so that the
// source window closes then presses escape to cancel the drag.
IN_PROC_BROWSER_TEST_F(DetachToBrowserTabDragControllerTest,
                       DeleteSourceDetached) {
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
  ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + tab_strip->height() + 20,
      base::Bind(&DeleteSourceDetachedStep2, to_delete)));
  // Schedule observer to quit message loop when done dragging. This has to be
  // async so the message loop can run.
  QuitWhenNotDragging();
  MessageLoop::current()->Run();

  // Should not be dragging.
  Browser* new_browser = BrowserList::GetLastActive();
  ASSERT_FALSE(GetTabStripForBrowser(new_browser)->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
}

namespace {

void PressEscapeWhileDetachedStep2() {
  // Cancel the drag.
  ui_controls::SendKeyPress(NULL, ui::VKEY_ESCAPE, false, false, false, false);
}

}  // namespace

// This is disabled until NativeViewHost::Detach really detaches.
// Detaches a tab and while detached presses escape to revert the drag.
IN_PROC_BROWSER_TEST_F(DetachToBrowserTabDragControllerTest,
                       DISABLED_PressEscapeWhileDetached) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_0_center));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));
  ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + tab_strip->height() + 20,
      base::Bind(&PressEscapeWhileDetachedStep2)));
  // Schedule observer to quit message loop when done dragging. This has to be
  // async so the message loop can run.
  QuitWhenNotDragging();
  MessageLoop::current()->Run();

  // Should not be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // And there should only be one window.
  EXPECT_EQ(1u, BrowserList::size());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));
}

namespace {

void DragAllStep2() {
  // Should only be one window.
  ASSERT_EQ(1u, BrowserList::size());
  // Release the mouse.
  ASSERT_TRUE(ui_controls::SendMouseEvents(
                  ui_controls::LEFT, ui_controls::UP));
}

}  // namespace

// Selects multiple tabs and starts dragging the window.
IN_PROC_BROWSER_TEST_F(DetachToBrowserTabDragControllerTest, DragAll) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  browser()->tab_strip_model()->AddTabAtToSelection(0);
  browser()->tab_strip_model()->AddTabAtToSelection(1);

  // Move to the first tab and drag it enough so that it would normally
  // detach.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(tab_0_center));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::DOWN));
  ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + tab_strip->height() + 20,
      base::Bind(&DragAllStep2)));
  // Schedule observer to quit message loop when done dragging. This has to be
  // async so the message loop can run.
  QuitWhenNotDragging();
  MessageLoop::current()->Run();

  // Should not be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // And there should only be one window.
  EXPECT_EQ(1u, BrowserList::size());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));
}

namespace {

// Invoked from the nested message loop.
void DragAllToSeparateWindowStep2(TabStrip* attached_tab_strip,
                                  TabStrip* target_tab_strip) {
  ASSERT_TRUE(attached_tab_strip->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, BrowserList::size());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  gfx::Point target_point(target_tab_strip->width() - 1,
                          target_tab_strip->height() / 2);
  views::View::ConvertPointToScreen(target_tab_strip, &target_point);
  ASSERT_TRUE(ui_controls::SendMouseMove(target_point.x(), target_point.y()));
}

}  // namespace

// Creates two browsers, selects all tabs in first and drags into second.
IN_PROC_BROWSER_TEST_F(DetachToBrowserTabDragControllerTest,
                       DragAllToSeparateWindow) {
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
  ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + tab_strip->height() + 20,
      base::Bind(&DragAllToSeparateWindowStep2, tab_strip, tab_strip2)));
  // Schedule observer to quit message loop when done dragging. This has to be
  // async so the message loop can run.
  QuitWhenNotDragging();
  MessageLoop::current()->Run();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, BrowserList::size());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
                  ui_controls::LEFT, ui_controls::UP));
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0 1", IDString(browser2->tab_strip_model()));
}

namespace {

// Invoked from the nested message loop.
void DragAllToSeparateWindowAndCancelStep2(TabStrip* attached_tab_strip,
                                           TabStrip* target_tab_strip) {
  ASSERT_TRUE(attached_tab_strip->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, BrowserList::size());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  gfx::Point target_point(target_tab_strip->width() - 1,
                          target_tab_strip->height() / 2);
  views::View::ConvertPointToScreen(target_tab_strip, &target_point);
  ASSERT_TRUE(ui_controls::SendMouseMove(target_point.x(), target_point.y()));
}

}  // namespace

// Creates two browsers, selects all tabs in first, drags into second, then hits
// escape.
IN_PROC_BROWSER_TEST_F(DetachToBrowserTabDragControllerTest,
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
  ASSERT_TRUE(ui_controls::SendMouseMoveNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + tab_strip->height() + 20,
                  base::Bind(&DragAllToSeparateWindowAndCancelStep2,
                             tab_strip, tab_strip2)));
  // Schedule observer to quit message loop when done dragging. This has to be
  // async so the message loop can run.
  QuitWhenNotDragging();
  MessageLoop::current()->Run();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, BrowserList::size());

  // Cancel the drag.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser2, ui::VKEY_ESCAPE, false, false, false, false));

  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0 1", IDString(browser2->tab_strip_model()));

  // browser() will have been destroyed, but browser2 should remain.
  ASSERT_EQ(1u, BrowserList::size());
}
