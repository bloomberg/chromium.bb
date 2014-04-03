// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_widget.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shelf_test_api.h"
#include "ash/test/shelf_view_test_api.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

ShelfWidget* GetShelfWidget() {
  return Shelf::ForPrimaryDisplay()->shelf_widget();
}

ShelfLayoutManager* GetShelfLayoutManager() {
  return GetShelfWidget()->shelf_layout_manager();
}

} // namespace

typedef test::AshTestBase ShelfWidgetTest;

void TestLauncherAlignment(aura::Window* root,
                           ShelfAlignment alignment,
                           const std::string& expected) {
  Shell::GetInstance()->SetShelfAlignment(alignment, root);
  gfx::Screen* screen = gfx::Screen::GetScreenFor(root);
  EXPECT_EQ(expected,
            screen->GetDisplayNearestWindow(root).work_area().ToString());
}

TEST_F(ShelfWidgetTest, TestAlignment) {
  Shelf* shelf = Shelf::ForPrimaryDisplay();
  UpdateDisplay("400x400");
  ASSERT_TRUE(shelf);
  {
    SCOPED_TRACE("Single Bottom");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(),
                          SHELF_ALIGNMENT_BOTTOM,
                          "0,0 400x353");
  }
  {
    SCOPED_TRACE("Single Right");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(),
                          SHELF_ALIGNMENT_RIGHT,
                          "0,0 353x400");
  }
  {
    SCOPED_TRACE("Single Left");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(),
                          SHELF_ALIGNMENT_LEFT,
                          "47,0 353x400");
  }
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x300,500x500");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  {
    SCOPED_TRACE("Primary Bottom");
    TestLauncherAlignment(root_windows[0],
                          SHELF_ALIGNMENT_BOTTOM,
                          "0,0 300x253");
  }
  {
    SCOPED_TRACE("Primary Right");
    TestLauncherAlignment(root_windows[0],
                          SHELF_ALIGNMENT_RIGHT,
                          "0,0 253x300");
  }
  {
    SCOPED_TRACE("Primary Left");
    TestLauncherAlignment(root_windows[0],
                          SHELF_ALIGNMENT_LEFT,
                          "47,0 253x300");
  }
  {
    SCOPED_TRACE("Secondary Bottom");
    TestLauncherAlignment(root_windows[1],
                          SHELF_ALIGNMENT_BOTTOM,
                          "300,0 500x453");
  }
  {
    SCOPED_TRACE("Secondary Right");
    TestLauncherAlignment(root_windows[1],
                          SHELF_ALIGNMENT_RIGHT,
                          "300,0 453x500");
  }
  {
    SCOPED_TRACE("Secondary Left");
    TestLauncherAlignment(root_windows[1],
                          SHELF_ALIGNMENT_LEFT,
                          "347,0 453x500");
  }
}

// Makes sure the shelf is initially sized correctly.
TEST_F(ShelfWidgetTest, LauncherInitiallySized) {
  ShelfWidget* shelf_widget = GetShelfWidget();
  Shelf* shelf = shelf_widget->shelf();
  ASSERT_TRUE(shelf);
  ShelfLayoutManager* shelf_layout_manager = GetShelfLayoutManager();
  ASSERT_TRUE(shelf_layout_manager);
  ASSERT_TRUE(shelf_widget->status_area_widget());
  int status_width = shelf_widget->status_area_widget()->
      GetWindowBoundsInScreen().width();
  // Test only makes sense if the status is > 0, which it better be.
  EXPECT_GT(status_width, 0);
  EXPECT_EQ(status_width, shelf_widget->GetContentsView()->width() -
            test::ShelfTestAPI(shelf).shelf_view()->width());
}

// Verifies when the shell is deleted with a full screen window we don't crash.
TEST_F(ShelfWidgetTest, DontReferenceShelfAfterDeletion) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->SetFullscreen(true);
}

#if defined(OS_CHROMEOS)
// Verifies shelf is created with correct size after user login and when its
// container and status widget has finished sizing.
// See http://crbug.com/252533
TEST_F(ShelfWidgetTest, ShelfInitiallySizedAfterLogin) {
  SetUserLoggedIn(false);
  UpdateDisplay("300x200,400x300");

  ShelfWidget* shelf_widget = NULL;
  Shell::RootWindowControllerList controllers(
      Shell::GetAllRootWindowControllers());
  for (Shell::RootWindowControllerList::const_iterator i = controllers.begin();
       i != controllers.end();
       ++i) {
    if (!(*i)->shelf()->shelf()) {
      shelf_widget = (*i)->shelf();
      break;
    }
  }
  ASSERT_TRUE(shelf_widget != NULL);

  SetUserLoggedIn(true);
  Shell::GetInstance()->CreateShelf();

  Shelf* shelf = shelf_widget->shelf();
  ASSERT_TRUE(shelf != NULL);

  const int status_width =
      shelf_widget->status_area_widget()->GetWindowBoundsInScreen().width();
  EXPECT_GT(status_width, 0);
  EXPECT_EQ(status_width,
            shelf_widget->GetContentsView()->width() -
                test::ShelfTestAPI(shelf).shelf_view()->width());
}
#endif  // defined(OS_CHROMEOS)

// Tests that the shelf lets mouse-events close to the edge fall through to the
// window underneath.
TEST_F(ShelfWidgetTest, ShelfEdgeOverlappingWindowHitTestMouse) {
  ShelfWidget* shelf_widget = GetShelfWidget();
  gfx::Rect shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(!shelf_bounds.IsEmpty());
  ShelfLayoutManager* shelf_layout_manager =
      shelf_widget->shelf_layout_manager();
  ASSERT_TRUE(shelf_layout_manager);
  EXPECT_EQ(SHELF_VISIBLE, shelf_layout_manager->visibility_state());

  // Create a Widget which overlaps with the shelf in the top edge.
  const int kOverlapSize = 15;
  const int kWindowHeight = 200;
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, shelf_bounds.y() - kWindowHeight + kOverlapSize,
                            200, kWindowHeight);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Show();
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(widget_bounds.Intersects(shelf_bounds));


  ui::EventTarget* root = widget->GetNativeWindow()->GetRootWindow();
  ui::EventTargeter* targeter = root->GetEventTargeter();
  {
    // Create a mouse-event targetting the top of the shelf widget. The
    // window-targeter should find |widget| as the target (instead of the
    // shelf).
    gfx::Point event_location(20, shelf_bounds.y() + 1);
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EF_NONE, ui::EF_NONE);
    ui::EventTarget* target = targeter->FindTargetForEvent(root, &mouse);
    EXPECT_EQ(widget->GetNativeWindow(), target);
  }

  // Now auto-hide (hidden) the shelf.
  shelf_layout_manager->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_layout_manager->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_layout_manager->auto_hide_state());
  shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(!shelf_bounds.IsEmpty());

  // Move |widget| so it still overlaps the shelf.
  widget->SetBounds(gfx::Rect(0, shelf_bounds.y() - kWindowHeight +
                              kOverlapSize, 200, kWindowHeight));
  widget_bounds = widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(widget_bounds.Intersects(shelf_bounds));
  {
    // Create a mouse-event targetting the top of the shelf widget. This time,
    // window-target should find the shelf as the target.
    gfx::Point event_location(20, shelf_bounds.y() + 1);
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EF_NONE, ui::EF_NONE);
    ui::EventTarget* target = targeter->FindTargetForEvent(root, &mouse);
    EXPECT_EQ(shelf_widget->GetNativeWindow(), target);
  }
}

// Tests that the shelf has a slightly larger hit-region for touch-events when
// it's in the auto-hidden state.
TEST_F(ShelfWidgetTest, HiddenShelfHitTestTouch) {
  ShelfWidget* shelf_widget = GetShelfWidget();
  gfx::Rect shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(!shelf_bounds.IsEmpty());
  ShelfLayoutManager* shelf_layout_manager =
      shelf_widget->shelf_layout_manager();
  ASSERT_TRUE(shelf_layout_manager);
  EXPECT_EQ(SHELF_VISIBLE, shelf_layout_manager->visibility_state());

  // Create a widget to make sure that the shelf does auto-hide.
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Show();

  ui::EventTarget* root = shelf_widget->GetNativeWindow()->GetRootWindow();
  ui::EventTargeter* targeter = root->GetEventTargeter();
  // Touch just over the shelf. Since the shelf is visible, the window-targeter
  // should not find the shelf as the target.
  {
    gfx::Point event_location(20, shelf_bounds.y() - 1);
    ui::TouchEvent touch(ui::ET_TOUCH_PRESSED, event_location, 0,
                         ui::EventTimeForNow());
    EXPECT_NE(shelf_widget->GetNativeWindow(),
              targeter->FindTargetForEvent(root, &touch));
  }

  // Now auto-hide (hidden) the shelf.
  shelf_layout_manager->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_layout_manager->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_layout_manager->auto_hide_state());
  shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(!shelf_bounds.IsEmpty());

  // Touch just over the shelf again. This time, the targeter should find the
  // shelf as the target.
  {
    gfx::Point event_location(20, shelf_bounds.y() - 1);
    ui::TouchEvent touch(ui::ET_TOUCH_PRESSED, event_location, 0,
                         ui::EventTimeForNow());
    EXPECT_EQ(shelf_widget->GetNativeWindow(),
              targeter->FindTargetForEvent(root, &touch));
  }
}

}  // namespace ash
