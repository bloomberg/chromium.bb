// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_drag_controller_interactive_uitest.h"

#include "ash/wm/property_util.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_controls.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/screen.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_ASH)
#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/wm/window_util.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#endif

using content::WebContents;

namespace test {

namespace {

const char kTabDragControllerInteractiveUITestUserDataKey[] =
    "TabDragControllerInteractiveUITestUserData";

class TabDragControllerInteractiveUITestUserData
    : public base::SupportsUserData::Data {
 public:
  explicit TabDragControllerInteractiveUITestUserData(int id) : id_(id) {}
  virtual ~TabDragControllerInteractiveUITestUserData() {}
  int id() { return id_; }

 private:
  int id_;
};

}  // namespace

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

void SetID(WebContents* web_contents, int id) {
  web_contents->SetUserData(&kTabDragControllerInteractiveUITestUserDataKey,
                            new TabDragControllerInteractiveUITestUserData(id));
}

void ResetIDs(TabStripModel* model, int start) {
  for (int i = 0; i < model->count(); ++i)
    SetID(model->GetWebContentsAt(i), start + i);
}

std::string IDString(TabStripModel* model) {
  std::string result;
  for (int i = 0; i < model->count(); ++i) {
    if (i != 0)
      result += " ";
    WebContents* contents = model->GetWebContentsAt(i);
    TabDragControllerInteractiveUITestUserData* user_data =
        static_cast<TabDragControllerInteractiveUITestUserData*>(
            contents->GetUserData(
                &kTabDragControllerInteractiveUITestUserDataKey));
    if (user_data)
      result += base::IntToString(user_data->id());
    else
      result += "?";
  }
  return result;
}

// Creates a listener that quits the message loop when no longer dragging.
void QuitWhenNotDraggingImpl() {
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
using test::GetTabStripForBrowser;

TabDragControllerTest::TabDragControllerTest()
    : native_browser_list(BrowserList::GetInstance(
                              chrome::HOST_DESKTOP_TYPE_NATIVE)) {
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
  // Create another browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  ResetIDs(browser2->tab_strip_model(), 100);

  // Resize the two windows so they're right next to each other.
  gfx::Rect work_area = gfx::Screen::GetNativeScreen()->GetDisplayNearestWindow(
      browser()->window()->GetNativeWindow()).work_area();
  gfx::Size half_size =
      gfx::Size(work_area.width() / 3 - 10, work_area.height() / 2 - 10);
  browser()->window()->SetBounds(gfx::Rect(work_area.origin(), half_size));
  browser2->window()->SetBounds(gfx::Rect(
      work_area.x() + half_size.width(), work_area.y(),
      half_size.width(), half_size.height()));
  return browser2;
}

namespace {

enum InputSource {
  INPUT_SOURCE_MOUSE = 0,
  INPUT_SOURCE_TOUCH = 1
};

int GetDetachY(TabStrip* tab_strip) {
  return std::max(TabDragController::kTouchVerticalDetachMagnetism,
                  TabDragController::kVerticalDetachMagnetism) +
      tab_strip->height() + 1;
}

bool GetTrackedByWorkspace(Browser* browser) {
#if !defined(USE_ASH) || defined(OS_WIN)  // TODO(win_ash)
  return true;
#else
  return ash::GetTrackedByWorkspace(browser->window()->GetNativeWindow());
#endif
}

}  // namespace

class DetachToBrowserTabDragControllerTest
    : public TabDragControllerTest,
      public ::testing::WithParamInterface<const char*> {
 public:
  DetachToBrowserTabDragControllerTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kTabBrowserDragging);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
#if defined(USE_ASH) && !defined(OS_WIN)  // TODO(win_ash)
    event_generator_.reset(new aura::test::EventGenerator(
                               ash::Shell::GetPrimaryRootWindow()));
#endif
  }

  InputSource input_source() const {
    return !strcmp(GetParam(), "mouse") ?
        INPUT_SOURCE_MOUSE : INPUT_SOURCE_TOUCH;
  }

  // The following methods update one of the mouse or touch input depending upon
  // the InputSource.
  bool PressInput(const gfx::Point& location) {
    if (input_source() == INPUT_SOURCE_MOUSE) {
      return ui_test_utils::SendMouseMoveSync(location) &&
          ui_test_utils::SendMouseEventsSync(
              ui_controls::LEFT, ui_controls::DOWN);
    }
#if defined(USE_ASH)  && !defined(OS_WIN)  // TODO(win_ash)
    event_generator_->set_current_location(location);
    event_generator_->PressTouch();
#else
    NOTREACHED();
#endif
    return true;
  }

  bool DragInputTo(const gfx::Point& location) {
    if (input_source() == INPUT_SOURCE_MOUSE)
      return ui_test_utils::SendMouseMoveSync(location);
#if defined(USE_ASH) && !defined(OS_WIN)  // TODO(win_ash)
    event_generator_->MoveTouch(location);
#else
    NOTREACHED();
#endif
    return true;
  }

  bool DragInputToAsync(const gfx::Point& location) {
    if (input_source() == INPUT_SOURCE_MOUSE)
      return ui_controls::SendMouseMove(location.x(), location.y());
#if defined(USE_ASH) && !defined(OS_WIN)  // TODO(win_ash)
    event_generator_->MoveTouch(location);
#else
    NOTREACHED();
#endif
    return true;
  }

  bool DragInputToNotifyWhenDone(int x,
                                 int y,
                                 const base::Closure& task) {
    if (input_source() == INPUT_SOURCE_MOUSE)
      return ui_controls::SendMouseMoveNotifyWhenDone(x, y, task);
#if defined(USE_ASH) && !defined(OS_WIN)  // TODO(win_ash)
    MessageLoop::current()->PostTask(FROM_HERE, task);
    event_generator_->MoveTouch(gfx::Point(x, y));
#else
    NOTREACHED();
#endif
    return true;
  }

  bool ReleaseInput() {
    if (input_source() == INPUT_SOURCE_MOUSE) {
      return ui_test_utils::SendMouseEventsSync(
              ui_controls::LEFT, ui_controls::UP);
    }
#if defined(USE_ASH) && !defined(OS_WIN)  // TODO(win_ash)
    event_generator_->ReleaseTouch();
#else
    NOTREACHED();
#endif
    return true;
  }

  bool ReleaseMouseAsync() {
    return input_source() == INPUT_SOURCE_MOUSE &&
        ui_controls::SendMouseEvents(ui_controls::LEFT, ui_controls::UP);
  }

  void QuitWhenNotDragging() {
    if (input_source() == INPUT_SOURCE_MOUSE) {
      // Schedule observer to quit message loop when done dragging. This has to
      // be async so the message loop can run.
      test::QuitWhenNotDraggingImpl();
      MessageLoop::current()->Run();
    } else {
      // Touch events are sync, so we know we're not in a drag session. But some
      // tests rely on the browser fully closing, which is async. So, run all
      // pending tasks.
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  void AddBlankTabAndShow(Browser* browser) {
    InProcessBrowserTest::AddBlankTabAndShow(browser);
  }

  Browser* browser() const { return InProcessBrowserTest::browser(); }

 private:
#if defined(USE_ASH) && !defined(OS_WIN)  // TODO(win_ash)
  scoped_ptr<aura::test::EventGenerator> event_generator_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DetachToBrowserTabDragControllerTest);
};

// Creates a browser with two tabs, drags the second to the first.
// TODO(sky): this won't work with touch as it requires a long press.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DISABLED_DragInSameWindow) {
  AddTabAndResetBrowser(browser());

  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  TabStripModel* model = browser()->tab_strip_model();

  gfx::Point tab_1_center(GetCenterInScreenCoordinates(tab_strip->tab_at(1)));
  ASSERT_TRUE(PressInput(tab_1_center));
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(DragInputTo(tab_0_center));
  ASSERT_TRUE(ReleaseInput());
  EXPECT_EQ("1 0", IDString(model));
  EXPECT_FALSE(TabDragController::IsActive());
  EXPECT_FALSE(tab_strip->IsDragSessionActive());
}

namespace {

// Invoked from the nested message loop.
void DragToSeparateWindowStep2(DetachToBrowserTabDragControllerTest* test,
                               TabStrip* not_attached_tab_strip,
                               TabStrip* target_tab_strip) {
  ASSERT_FALSE(not_attached_tab_strip->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  gfx::Point target_point(target_tab_strip->width() -1,
                          target_tab_strip->height() / 2);
  views::View::ConvertPointToScreen(target_tab_strip, &target_point);
  ASSERT_TRUE(test->DragInputToAsync(target_point));
}

}  // namespace

// Creates two browsers, drags from first into second.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragToSeparateWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&DragToSeparateWindowStep2,
                             this, tab_strip, tab_strip2)));
  QuitWhenNotDragging();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  EXPECT_TRUE(GetTrackedByWorkspace(browser()));

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
  EXPECT_TRUE(GetTrackedByWorkspace(browser2));
}

namespace {

void DetachToOwnWindowStep2(DetachToBrowserTabDragControllerTest* test) {
  if (test->input_source() == INPUT_SOURCE_TOUCH)
    ASSERT_TRUE(test->ReleaseInput());
}

#if defined(USE_ASH) && !defined(OS_WIN)  // TODO(win_ash)
bool IsWindowPositionManaged(aura::Window* window) {
  return ash::wm::IsWindowPositionManaged(window);
}
bool HasUserChangedWindowPositionOrSize(aura::Window* window) {
  return ash::wm::HasUserChangedWindowPositionOrSize(window);
}
#else
bool IsWindowPositionManaged(gfx::NativeWindow window) {
  return true;
}
bool HasUserChangedWindowPositionOrSize(gfx::NativeWindow window) {
  return false;
}
#endif

}  // namespace

// Drags from browser to separate window and releases mouse.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DetachToOwnWindow) {
  const gfx::Rect initial_bounds(browser()->window()->GetBounds());
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&DetachToOwnWindowStep2, this)));
  if (input_source() == INPUT_SOURCE_MOUSE) {
    ASSERT_TRUE(ReleaseMouseAsync());
    QuitWhenNotDragging();
  }

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, native_browser_list->size());
  Browser* new_browser = native_browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  // The bounds of the initial window should not have changed.
  EXPECT_EQ(initial_bounds.ToString(),
            browser()->window()->GetBounds().ToString());

  EXPECT_TRUE(GetTrackedByWorkspace(browser()));
  EXPECT_TRUE(GetTrackedByWorkspace(new_browser));
  // After this both windows should still be managable.
  EXPECT_TRUE(IsWindowPositionManaged(browser()->window()->GetNativeWindow()));
  EXPECT_TRUE(IsWindowPositionManaged(
      new_browser->window()->GetNativeWindow()));
}

// Deletes a tab being dragged before the user moved enough to start a drag.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DeleteBeforeStartedDragging) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Click on the first tab, but don't move it.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));

  // Should be dragging.
  ASSERT_TRUE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Delete the tab being dragged.
  delete browser()->tab_strip_model()->GetWebContentsAt(0);

  // Should have canceled dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
  EXPECT_TRUE(GetTrackedByWorkspace(browser()));
}

// Deletes a tab being dragged while still attached.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DeleteTabWhileAttached) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Click on the first tab and move it enough so that it starts dragging but is
  // still attached.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputTo(
                  gfx::Point(tab_0_center.x() + 20, tab_0_center.y())));

  // Should be dragging.
  ASSERT_TRUE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Delete the tab being dragged.
  delete browser()->tab_strip_model()->GetWebContentsAt(0);

  // Should have canceled dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  EXPECT_TRUE(GetTrackedByWorkspace(browser()));
}

namespace {

void DeleteWhileDetachedStep2(WebContents* tab) {
  delete tab;
}

}  // namespace

// Deletes a tab being dragged after dragging a tab so that a new window is
// created.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DeleteTabWhileDetached) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  WebContents* to_delete =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&DeleteWhileDetachedStep2, to_delete)));
  QuitWhenNotDragging();

  // Should not be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  EXPECT_TRUE(GetTrackedByWorkspace(browser()));
}

namespace {

void DeleteSourceDetachedStep2(WebContents* tab,
                               const BrowserList* browser_list) {
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  // This ends up closing the source window.
  delete tab;
  // Cancel the drag.
  ui_controls::SendKeyPress(new_browser->window()->GetNativeWindow(),
                            ui::VKEY_ESCAPE, false, false, false, false);
}

}  // namespace

// Detaches a tab and while detached deletes a tab from the source so that the
// source window closes then presses escape to cancel the drag.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DeleteSourceDetached) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  WebContents* to_delete = browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&DeleteSourceDetachedStep2, to_delete, native_browser_list)));
  QuitWhenNotDragging();

  // Should not be dragging.
  ASSERT_EQ(1u, native_browser_list->size());
  Browser* new_browser = native_browser_list->get(0);
  ASSERT_FALSE(GetTabStripForBrowser(new_browser)->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));

  EXPECT_TRUE(GetTrackedByWorkspace(new_browser));
}

namespace {

void PressEscapeWhileDetachedStep2(const BrowserList* browser_list) {
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  ui_controls::SendKeyPress(
      new_browser->window()->GetNativeWindow(), ui::VKEY_ESCAPE, false, false,
      false, false);
}

}  // namespace

// This is disabled until NativeViewHost::Detach really detaches.
// Detaches a tab and while detached presses escape to revert the drag.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       PressEscapeWhileDetached) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&PressEscapeWhileDetachedStep2, native_browser_list)));
  QuitWhenNotDragging();

  // Should not be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // And there should only be one window.
  EXPECT_EQ(1u, native_browser_list->size());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));
}

namespace {

void DragAllStep2(DetachToBrowserTabDragControllerTest* test,
                  const BrowserList* browser_list) {
  // Should only be one window.
  ASSERT_EQ(1u, browser_list->size());
  if (test->input_source() == INPUT_SOURCE_TOUCH) {
    ASSERT_TRUE(test->ReleaseInput());
  } else {
    ASSERT_TRUE(test->ReleaseMouseAsync());
  }
}

}  // namespace

// Selects multiple tabs and starts dragging the window.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest, DragAll) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());
  browser()->tab_strip_model()->AddTabAtToSelection(0);
  browser()->tab_strip_model()->AddTabAtToSelection(1);

  // Move to the first tab and drag it enough so that it would normally
  // detach.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&DragAllStep2, this, native_browser_list)));
  QuitWhenNotDragging();

  // Should not be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // And there should only be one window.
  EXPECT_EQ(1u, native_browser_list->size());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  EXPECT_TRUE(GetTrackedByWorkspace(browser()));
}

namespace {

// Invoked from the nested message loop.
void DragAllToSeparateWindowStep2(DetachToBrowserTabDragControllerTest* test,
                                  TabStrip* attached_tab_strip,
                                  TabStrip* target_tab_strip,
                                  const BrowserList* browser_list) {
  ASSERT_TRUE(attached_tab_strip->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  gfx::Point target_point(target_tab_strip->width() - 1,
                          target_tab_strip->height() / 2);
  views::View::ConvertPointToScreen(target_tab_strip, &target_point);
  ASSERT_TRUE(test->DragInputToAsync(target_point));
}

}  // namespace

// Creates two browsers, selects all tabs in first and drags into second.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragAllToSeparateWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  browser()->tab_strip_model()->AddTabAtToSelection(0);
  browser()->tab_strip_model()->AddTabAtToSelection(1);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&DragAllToSeparateWindowStep2, this, tab_strip, tab_strip2,
                 native_browser_list)));
  QuitWhenNotDragging();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, native_browser_list->size());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0 1", IDString(browser2->tab_strip_model()));

  EXPECT_TRUE(GetTrackedByWorkspace(browser2));
}

namespace {

// Invoked from the nested message loop.
void DragAllToSeparateWindowAndCancelStep2(
    DetachToBrowserTabDragControllerTest* test,
    TabStrip* attached_tab_strip,
    TabStrip* target_tab_strip,
    const BrowserList* browser_list) {
  ASSERT_TRUE(attached_tab_strip->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  gfx::Point target_point(target_tab_strip->width() - 1,
                          target_tab_strip->height() / 2);
  views::View::ConvertPointToScreen(target_tab_strip, &target_point);
  ASSERT_TRUE(test->DragInputToAsync(target_point));
}

}  // namespace

// Creates two browsers, selects all tabs in first, drags into second, then hits
// escape.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragAllToSeparateWindowAndCancel) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  browser()->tab_strip_model()->AddTabAtToSelection(0);
  browser()->tab_strip_model()->AddTabAtToSelection(1);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&DragAllToSeparateWindowAndCancelStep2, this,
                             tab_strip, tab_strip2, native_browser_list)));
  QuitWhenNotDragging();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, native_browser_list->size());

  // Cancel the drag.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser2, ui::VKEY_ESCAPE, false, false, false, false));

  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0 1", IDString(browser2->tab_strip_model()));

  // browser() will have been destroyed, but browser2 should remain.
  ASSERT_EQ(1u, native_browser_list->size());

  EXPECT_TRUE(GetTrackedByWorkspace(browser2));
}

// Creates two browsers, drags from first into the second in such a way that
// no detaching should happen.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragDirectlyToSecondWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);

  // Move the tabstrip down enough so that we can detach.
  gfx::Rect bounds(browser2->window()->GetBounds());
  bounds.Offset(0, 100);
  browser2->window()->SetBounds(bounds);

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));

  gfx::Point b2_location(5, 0);
  views::View::ConvertPointToScreen(tab_strip2, &b2_location);
  ASSERT_TRUE(DragInputTo(b2_location));

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));

  EXPECT_TRUE(GetTrackedByWorkspace(browser()));
  EXPECT_TRUE(GetTrackedByWorkspace(browser2));
}

// Creates two browsers, the first browser has a single tab and drags into the
// second browser.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragSingleTabToSeparateWindow) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  ResetIDs(browser()->tab_strip_model(), 0);

  // Create another browser.
  Browser* browser2 = CreateAnotherWindowBrowserAndRelayout();
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  const gfx::Rect initial_bounds(browser2->window()->GetBounds());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&DragAllToSeparateWindowStep2, this, tab_strip, tab_strip2,
                 native_browser_list)));
  QuitWhenNotDragging();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(1u, native_browser_list->size());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("100 0", IDString(browser2->tab_strip_model()));

  EXPECT_TRUE(GetTrackedByWorkspace(browser2));
  // Make sure that the window is still managed and not user moved.
  EXPECT_TRUE(IsWindowPositionManaged(browser2->window()->GetNativeWindow()));
  EXPECT_FALSE(HasUserChangedWindowPositionOrSize(
      browser2->window()->GetNativeWindow()));
  // Also make sure that the drag to window position has not changed.
  EXPECT_EQ(initial_bounds.ToString(),
            browser2->window()->GetBounds().ToString());
}

namespace {

// Invoked from the nested message loop.
void CancelOnNewTabWhenDraggingStep2(
    DetachToBrowserTabDragControllerTest* test,
    const BrowserList* browser_list) {
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  // Add another tab. This should trigger exiting the nested loop.
  test->AddBlankTabAndShow(browser_list->GetLastActive());
}

}  // namespace

// Adds another tab, detaches into separate window, adds another tab and
// verifies the run loop ends.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       CancelOnNewTabWhenDragging) {
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Add another tab to browser().
  AddTabAndResetBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&CancelOnNewTabWhenDraggingStep2, this, native_browser_list)));
  QuitWhenNotDragging();

  // Should be two windows and not dragging.
  ASSERT_FALSE(TabDragController::IsActive());
  ASSERT_EQ(2u, native_browser_list->size());
  for (chrome::BrowserIterator it; !it.done(); it.Next())
    EXPECT_TRUE(GetTrackedByWorkspace(*it));
}

#if defined(USE_ASH) && !defined(OS_WIN)  // TODO(win_ash)

namespace {

void DragInMaximizedWindowStep2(DetachToBrowserTabDragControllerTest* test,
                                Browser* browser,
                                TabStrip* tab_strip,
                                const BrowserList* browser_list) {
  // There should be another browser.
  ASSERT_EQ(2u, browser_list->size());
  Browser* new_browser = browser_list->get(1);
  EXPECT_NE(browser, new_browser);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);

  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());

  // Both windows should be visible.
  EXPECT_TRUE(tab_strip->GetWidget()->IsVisible());
  EXPECT_TRUE(tab_strip2->GetWidget()->IsVisible());

  // Stops dragging.
  ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// Creates a browser with two tabs, maximizes it, drags the tab out.
IN_PROC_BROWSER_TEST_P(DetachToBrowserTabDragControllerTest,
                       DragInMaximizedWindow) {
  AddTabAndResetBrowser(browser());
  browser()->window()->Maximize();

  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
      tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
      base::Bind(&DragInMaximizedWindowStep2, this, browser(), tab_strip,
                 native_browser_list)));
  QuitWhenNotDragging();

  ASSERT_FALSE(TabDragController::IsActive());

  // Should be two browsers.
  ASSERT_EQ(2u, native_browser_list->size());
  Browser* new_browser = native_browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());

  // Only the new browser should be visible.
  EXPECT_FALSE(browser()->window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(new_browser->window()->GetNativeWindow()->IsVisible());

  EXPECT_TRUE(GetTrackedByWorkspace(browser()));
  EXPECT_TRUE(GetTrackedByWorkspace(new_browser));
}

// Subclass of DetachToBrowserInSeparateDisplayTabDragControllerTest that
// creates multiple displays.
class DetachToBrowserInSeparateDisplayTabDragControllerTest
    : public DetachToBrowserTabDragControllerTest {
 public:
  DetachToBrowserInSeparateDisplayTabDragControllerTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    DetachToBrowserTabDragControllerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "0+0-250x250,251+0-250x250");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DetachToBrowserInSeparateDisplayTabDragControllerTest);
};

namespace {

void DragSingleTabToSeparateWindowInSecondDisplayStep2(
    DetachToBrowserTabDragControllerTest* test,
    const gfx::Point& target_point) {
  ASSERT_TRUE(test->DragInputTo(target_point));
  if (test->input_source() == INPUT_SOURCE_TOUCH)
    ASSERT_TRUE(test->ReleaseInput());
}

}  // namespace

// Drags from browser to a second display and releases input.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DragSingleTabToSeparateWindowInSecondDisplay) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(
      GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&DragSingleTabToSeparateWindowInSecondDisplayStep2,
                             this, gfx::Point(tab_0_center.x(), 250 +
                                              GetDetachY(tab_strip)))));
  if (input_source() == INPUT_SOURCE_MOUSE) {
    ASSERT_TRUE(ReleaseMouseAsync());
    QuitWhenNotDragging();
  }

  // Should no longer be dragging.
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());

  // There should now be another browser.
  ASSERT_EQ(2u, native_browser_list->size());
  Browser* new_browser = native_browser_list->get(1);
  ASSERT_TRUE(new_browser->window()->IsActive());
  TabStrip* tab_strip2 = GetTabStripForBrowser(new_browser);
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());

  EXPECT_EQ("0", IDString(new_browser->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
}

namespace {

// Invoked from the nested message loop.
void DragTabToWindowInSeparateDisplayStep2(
    DetachToBrowserTabDragControllerTest* test,
    TabStrip* not_attached_tab_strip,
    TabStrip* target_tab_strip) {
  ASSERT_FALSE(not_attached_tab_strip->IsDragSessionActive());
  ASSERT_FALSE(target_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Drag to target_tab_strip. This should stop the nested loop from dragging
  // the window.
  gfx::Point target_point(
      GetCenterInScreenCoordinates(target_tab_strip->tab_at(0)));

  // Move it close to the beginning of the target tabstrip.
  target_point.set_x(
      target_point.x() - target_tab_strip->tab_at(0)->width() / 2 + 10);
  ASSERT_TRUE(test->DragInputToAsync(target_point));
}

}  // namespace

// Drags from browser to a second display and releases input.
IN_PROC_BROWSER_TEST_P(DetachToBrowserInSeparateDisplayTabDragControllerTest,
                       DragTabToWindowInSeparateDisplay) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Create another browser.
  Browser* browser2 = CreateBrowser(browser()->profile());
  TabStrip* tab_strip2 = GetTabStripForBrowser(browser2);
  ResetIDs(browser2->tab_strip_model(), 100);

  // Move the second browser to the second display.
  std::vector<aura::RootWindow*> roots(ash::Shell::GetAllRootWindows());
  ASSERT_EQ(2u, roots.size());
  aura::RootWindow* second_root = roots[1];
  gfx::Rect work_area = gfx::Screen::GetNativeScreen()->GetDisplayNearestWindow(
      second_root).work_area();
  browser2->window()->SetBounds(work_area);
  EXPECT_EQ(second_root,
            browser2->window()->GetNativeWindow()->GetRootWindow());

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough that it attaches to browser2.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&DragTabToWindowInSeparateDisplayStep2,
                             this, tab_strip, tab_strip2)));
  QuitWhenNotDragging();

  // Should now be attached to tab_strip2.
  ASSERT_TRUE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  // Release the mouse, stopping the drag session.
  ASSERT_TRUE(ReleaseInput());
  ASSERT_FALSE(tab_strip2->IsDragSessionActive());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 100", IDString(browser2->tab_strip_model()));
  EXPECT_EQ("1", IDString(browser()->tab_strip_model()));
}

class DifferentDeviceScaleFactorDisplayTabDragControllerTest
    : public DetachToBrowserTabDragControllerTest {
 public:
  DifferentDeviceScaleFactorDisplayTabDragControllerTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    DetachToBrowserTabDragControllerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "400x400,800x800*2");
  }

  float GetCursorDeviceScaleFactor() const {
    ash::test::CursorManagerTestApi cursor_test_api(
        ash::Shell::GetInstance()->cursor_manager());
    return cursor_test_api.GetDeviceScaleFactor();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DifferentDeviceScaleFactorDisplayTabDragControllerTest);
};

namespace {

// The points where a tab is dragged in CursorDeviceScaleFactorStep.
const struct DragPoint {
  int x;
  int y;
} kDragPoints[] = {
  {300, 200},
  {399, 200},
  {500, 200},
  {400, 200},
  {300, 200},
};

// The expected device scale factors before the cursor is moved to the
// corresponding kDragPoints in CursorDeviceScaleFactorStep.
const float kDeviceScaleFactorExpectations[] = {
  1.0f,
  1.0f,
  2.0f,
  2.0f,
  1.0f,
};

COMPILE_ASSERT(
    arraysize(kDragPoints) == arraysize(kDeviceScaleFactorExpectations),
    kDragPoints_and_kDeviceScaleFactorExpectations_must_have_same_size);

// Drags tab to |kDragPoints[index]|, then calls the next step function.
void CursorDeviceScaleFactorStep(
    DifferentDeviceScaleFactorDisplayTabDragControllerTest* test,
    TabStrip* not_attached_tab_strip,
    size_t index) {
  ASSERT_FALSE(not_attached_tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());

  if (index < arraysize(kDragPoints)) {
    EXPECT_EQ(kDeviceScaleFactorExpectations[index],
              test->GetCursorDeviceScaleFactor());
    const DragPoint p = kDragPoints[index];
    ASSERT_TRUE(test->DragInputToNotifyWhenDone(
        p.x, p.y, base::Bind(&CursorDeviceScaleFactorStep,
                             test, not_attached_tab_strip, index + 1)));
  } else {
    // Finishes a serise of CursorDeviceScaleFactorStep calls and ends drag.
    EXPECT_EQ(1.0f, test->GetCursorDeviceScaleFactor());
    ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
        ui_controls::LEFT, ui_controls::UP));
  }
}

}  // namespace

// Verifies cursor's device scale factor is updated when a tab is moved across
// displays with different device scale factors (http://crbug.com/154183).
IN_PROC_BROWSER_TEST_P(DifferentDeviceScaleFactorDisplayTabDragControllerTest,
                       CursorDeviceScaleFactor) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  // Move the second browser to the second display.
  std::vector<aura::RootWindow*> roots(ash::Shell::GetAllRootWindows());
  ASSERT_EQ(2u, roots.size());

  // Move to the first tab and drag it enough so that it detaches.
  gfx::Point tab_0_center(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(PressInput(tab_0_center));
  ASSERT_TRUE(DragInputToNotifyWhenDone(
                  tab_0_center.x(), tab_0_center.y() + GetDetachY(tab_strip),
                  base::Bind(&CursorDeviceScaleFactorStep,
                             this, tab_strip, 0)));
  QuitWhenNotDragging();
}

namespace {

class DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest
    : public TabDragControllerTest {
 public:
  DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    TabDragControllerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "0+0-250x250,251+0-250x250");
  }

  bool Press(const gfx::Point& position) {
    return ui_test_utils::SendMouseMoveSync(position) &&
        ui_test_utils::SendMouseEventsSync(ui_controls::LEFT,
                                           ui_controls::DOWN);
  }

  bool DragTabAndExecuteTaskWhenDone(const gfx::Point& position,
                                     const base::Closure& task) {
    return ui_controls::SendMouseMoveNotifyWhenDone(
        position.x(), position.y(), task);
  }

  void QuitWhenNotDragging() {
    test::QuitWhenNotDraggingImpl();
    MessageLoop::current()->Run();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest);
};

// Invoked from the nested message loop.
void CancelDragTabToWindowInSeparateDisplayStep3(
    TabStrip* tab_strip,
    const BrowserList* browser_list) {
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  // Switching display mode should cancel the drag operation.
  ash::internal::DisplayManager::CycleDisplay();
}

// Invoked from the nested message loop.
void CancelDragTabToWindowInSeparateDisplayStep2(
    DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest* test,
    TabStrip* tab_strip,
    aura::RootWindow* current_root,
    gfx::Point final_destination,
    const BrowserList* browser_list) {
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_TRUE(TabDragController::IsActive());
  ASSERT_EQ(2u, browser_list->size());

  Browser* new_browser = browser_list->get(1);
  EXPECT_EQ(current_root,
            new_browser->window()->GetNativeWindow()->GetRootWindow());

  ASSERT_TRUE(test->DragTabAndExecuteTaskWhenDone(
      final_destination,
      base::Bind(&CancelDragTabToWindowInSeparateDisplayStep3,
                 tab_strip, browser_list)));
}

}  // namespace

// Drags from browser to a second display and releases input.
IN_PROC_BROWSER_TEST_F(
    DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest,
    CancelDragTabToWindowIn2ndDisplay) {
  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Move the second browser to the second display.
  std::vector<aura::RootWindow*> roots(ash::Shell::GetAllRootWindows());
  ASSERT_EQ(2u, roots.size());
  gfx::Point final_destination =
      gfx::Screen::GetNativeScreen()->GetDisplayNearestWindow(
          roots[1]).work_area().CenterPoint();

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough to move to another display.
  gfx::Point tab_0_dst(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(Press(tab_0_dst));
  tab_0_dst.Offset(0, GetDetachY(tab_strip));
  ASSERT_TRUE(DragTabAndExecuteTaskWhenDone(
      tab_0_dst, base::Bind(&CancelDragTabToWindowInSeparateDisplayStep2,
                            this, tab_strip, roots[0], final_destination,
                            native_browser_list)));
  QuitWhenNotDragging();

  ASSERT_EQ(1u, native_browser_list->size());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Release the mouse
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
      ui_controls::LEFT, ui_controls::UP));
}

// Drags from browser from a second display to primary and releases input.
IN_PROC_BROWSER_TEST_F(
    DetachToBrowserInSeparateDisplayAndCancelTabDragControllerTest,
    CancelDragTabToWindowIn1stDisplay) {
  std::vector<aura::RootWindow*> roots(ash::Shell::GetAllRootWindows());
  ASSERT_EQ(2u, roots.size());

  // Add another tab.
  AddTabAndResetBrowser(browser());
  TabStrip* tab_strip = GetTabStripForBrowser(browser());

  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));
  EXPECT_EQ(roots[0], browser()->window()->GetNativeWindow()->GetRootWindow());

  gfx::Rect work_area = gfx::Screen::GetNativeScreen()->
      GetDisplayNearestWindow(roots[1]).work_area();
  browser()->window()->SetBounds(work_area);
  EXPECT_EQ(roots[1], browser()->window()->GetNativeWindow()->GetRootWindow());

  // Move the second browser to the display.
  gfx::Point final_destination =
      gfx::Screen::GetNativeScreen()->GetDisplayNearestWindow(
          roots[0]).work_area().CenterPoint();

  // Move to the first tab and drag it enough so that it detaches, but not
  // enough to move to another display.
  gfx::Point tab_0_dst(GetCenterInScreenCoordinates(tab_strip->tab_at(0)));
  ASSERT_TRUE(Press(tab_0_dst));
  tab_0_dst.Offset(0, GetDetachY(tab_strip));
  ASSERT_TRUE(DragTabAndExecuteTaskWhenDone(
      tab_0_dst, base::Bind(&CancelDragTabToWindowInSeparateDisplayStep2,
                            this, tab_strip, roots[1], final_destination,
                            native_browser_list)));
  QuitWhenNotDragging();

  ASSERT_EQ(1u, native_browser_list->size());
  ASSERT_FALSE(tab_strip->IsDragSessionActive());
  ASSERT_FALSE(TabDragController::IsActive());
  EXPECT_EQ("0 1", IDString(browser()->tab_strip_model()));

  // Release the mouse
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
      ui_controls::LEFT, ui_controls::UP));
}

#endif

#if defined(USE_ASH) && !defined(OS_WIN)  // TODO(win_ash)
INSTANTIATE_TEST_CASE_P(TabDragging,
                        DetachToBrowserInSeparateDisplayTabDragControllerTest,
                        ::testing::Values("mouse", "touch"));
INSTANTIATE_TEST_CASE_P(TabDragging,
                        DifferentDeviceScaleFactorDisplayTabDragControllerTest,
                        ::testing::Values("mouse"));
INSTANTIATE_TEST_CASE_P(TabDragging,
                        DetachToBrowserTabDragControllerTest,
                        ::testing::Values("mouse", "touch"));
#else
INSTANTIATE_TEST_CASE_P(TabDragging,
                        DetachToBrowserTabDragControllerTest,
                        ::testing::Values("mouse"));
#endif
