// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_widget.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/test/ash_md_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/shelf_test_api.h"
#include "ash/test/shelf_view_test_api.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
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

}  // namespace

using ShelfWidgetTest = test::AshMDTestBase;

INSTANTIATE_TEST_CASE_P(
    /* prefix intentionally left blank due to only one parameterization */,
    ShelfWidgetTest,
    testing::Values(MaterialDesignController::NON_MATERIAL,
                    MaterialDesignController::MATERIAL_NORMAL,
                    MaterialDesignController::MATERIAL_EXPERIMENTAL));

void TestLauncherAlignment(aura::Window* root,
                           ShelfAlignment alignment,
                           const std::string& expected) {
  Shelf::ForWindow(root)->SetAlignment(alignment);
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(expected,
            screen->GetDisplayNearestWindow(root).work_area().ToString());
}

// TODO(msw): Broken on Windows. http://crbug.com/584038
#if defined(OS_CHROMEOS)
TEST_P(ShelfWidgetTest, TestAlignment) {
  // Note that for a left- and right-aligned shelf, this offset must be
  // applied to a maximized window's width rather than its height.
  const int offset = GetMdMaximizedWindowHeightOffset();
  const int kShelfSize = GetShelfConstant(SHELF_SIZE);
  UpdateDisplay("400x400");
  {
    SCOPED_TRACE("Single Bottom");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(), SHELF_ALIGNMENT_BOTTOM,
                          gfx::Rect(0, 0, 400, 353 + offset).ToString());
  }
  {
    SCOPED_TRACE("Single Locked");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(),
                          SHELF_ALIGNMENT_BOTTOM_LOCKED,
                          gfx::Rect(0, 0, 400, 353 + offset).ToString());
  }
  {
    SCOPED_TRACE("Single Right");
    TestLauncherAlignment(Shell::GetPrimaryRootWindow(), SHELF_ALIGNMENT_RIGHT,
                          gfx::Rect(0, 0, 353 + offset, 400).ToString());
  }
  {
    SCOPED_TRACE("Single Left");
    TestLauncherAlignment(
        Shell::GetPrimaryRootWindow(), SHELF_ALIGNMENT_LEFT,
        gfx::Rect(kShelfSize, 0, 353 + offset, 400).ToString());
  }
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x300,500x500");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  {
    SCOPED_TRACE("Primary Bottom");
    TestLauncherAlignment(root_windows[0], SHELF_ALIGNMENT_BOTTOM,
                          gfx::Rect(0, 0, 300, 253 + offset).ToString());
  }
  {
    SCOPED_TRACE("Primary Locked");
    TestLauncherAlignment(root_windows[0], SHELF_ALIGNMENT_BOTTOM_LOCKED,
                          gfx::Rect(0, 0, 300, 253 + offset).ToString());
  }
  {
    SCOPED_TRACE("Primary Right");
    TestLauncherAlignment(root_windows[0], SHELF_ALIGNMENT_RIGHT,
                          gfx::Rect(0, 0, 253 + offset, 300).ToString());
  }
  {
    SCOPED_TRACE("Primary Left");
    TestLauncherAlignment(
        root_windows[0], SHELF_ALIGNMENT_LEFT,
        gfx::Rect(kShelfSize, 0, 253 + offset, 300).ToString());
  }
  {
    SCOPED_TRACE("Secondary Bottom");
    TestLauncherAlignment(root_windows[1], SHELF_ALIGNMENT_BOTTOM,
                          gfx::Rect(300, 0, 500, 453 + offset).ToString());
  }
  {
    SCOPED_TRACE("Secondary Locked");
    TestLauncherAlignment(root_windows[1], SHELF_ALIGNMENT_BOTTOM_LOCKED,
                          gfx::Rect(300, 0, 500, 453 + offset).ToString());
  }
  {
    SCOPED_TRACE("Secondary Right");
    TestLauncherAlignment(root_windows[1], SHELF_ALIGNMENT_RIGHT,
                          gfx::Rect(300, 0, 453 + offset, 500).ToString());
  }
  {
    SCOPED_TRACE("Secondary Left");
    TestLauncherAlignment(
        root_windows[1], SHELF_ALIGNMENT_LEFT,
        gfx::Rect(300 + kShelfSize, 0, 453 + offset, 500).ToString());
  }
}
#endif  // defined(OS_CHROMEOS)

// Makes sure the shelf is initially sized correctly.
TEST_P(ShelfWidgetTest, LauncherInitiallySized) {
  ShelfWidget* shelf_widget = GetShelfWidget();
  Shelf* shelf = shelf_widget->shelf();
  ASSERT_TRUE(shelf);
  ShelfLayoutManager* shelf_layout_manager = GetShelfLayoutManager();
  ASSERT_TRUE(shelf_layout_manager);
  ASSERT_TRUE(shelf_widget->status_area_widget());
  int status_width =
      shelf_widget->status_area_widget()->GetWindowBoundsInScreen().width();
  // Test only makes sense if the status is > 0, which it better be.
  EXPECT_GT(status_width, 0);
  EXPECT_EQ(status_width, shelf_widget->GetContentsView()->width() -
                              test::ShelfTestAPI(shelf).shelf_view()->width());
}

// Verifies when the shell is deleted with a full screen window we don't crash.
TEST_P(ShelfWidgetTest, DontReferenceShelfAfterDeletion) {
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
TEST_P(ShelfWidgetTest, ShelfInitiallySizedAfterLogin) {
  SetUserLoggedIn(false);
  UpdateDisplay("300x200,400x300");

  ShelfWidget* shelf_widget = NULL;
  Shell::RootWindowControllerList controllers(
      Shell::GetAllRootWindowControllers());
  for (Shell::RootWindowControllerList::const_iterator i = controllers.begin();
       i != controllers.end(); ++i) {
    if (!(*i)->shelf_widget()->shelf()) {
      shelf_widget = (*i)->shelf_widget();
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
  EXPECT_EQ(status_width, shelf_widget->GetContentsView()->width() -
                              test::ShelfTestAPI(shelf).shelf_view()->width());
}
#endif  // defined(OS_CHROMEOS)

// Tests that the shelf lets mouse-events close to the edge fall through to the
// window underneath.
TEST_P(ShelfWidgetTest, ShelfEdgeOverlappingWindowHitTestMouse) {
  if (!SupportsHostWindowResize())
    return;

  UpdateDisplay("400x400");
  ShelfWidget* shelf_widget = GetShelfWidget();
  gfx::Rect shelf_bounds = shelf_widget->GetWindowBoundsInScreen();

  EXPECT_TRUE(!shelf_bounds.IsEmpty());
  ShelfLayoutManager* shelf_layout_manager =
      shelf_widget->shelf_layout_manager();
  ASSERT_TRUE(shelf_layout_manager);
  EXPECT_EQ(SHELF_VISIBLE, shelf_layout_manager->visibility_state());

  // Create a Widget which overlaps the shelf in both left and bottom
  // alignments.
  const int kOverlapSize = 15;
  const int kWindowHeight = 200;
  const int kWindowWidth = 200;
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(shelf_bounds.height() - kOverlapSize,
                            shelf_bounds.y() - kWindowHeight + kOverlapSize,
                            kWindowWidth, kWindowHeight);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Show();
  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(widget_bounds.Intersects(shelf_bounds));

  ui::EventTarget* root = widget->GetNativeWindow()->GetRootWindow();
  ui::EventTargeter* targeter = root->GetEventTargeter();
  {
    // Create a mouse-event targeting the top of the shelf widget. The
    // window-targeter should find |widget| as the target (instead of the
    // shelf).
    gfx::Point event_location(widget_bounds.x() + 5, shelf_bounds.y() + 1);
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
    ui::EventTarget* target = targeter->FindTargetForEvent(root, &mouse);
    EXPECT_EQ(widget->GetNativeWindow(), target);
  }

  // Change shelf alignment to verify that the targeter insets are updated.
  Shelf* shelf = Shelf::ForPrimaryDisplay();
  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  shelf_layout_manager->LayoutShelf();
  shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  {
    // Create a mouse-event targeting the right edge of the shelf widget. The
    // window-targeter should find |widget| as the target (instead of the
    // shelf).
    gfx::Point event_location(shelf_bounds.right() - 1, widget_bounds.y() + 5);
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
    ui::EventTarget* target = targeter->FindTargetForEvent(root, &mouse);
    EXPECT_EQ(widget->GetNativeWindow(), target);
  }

  // Now restore shelf alignment (bottom) and auto-hide (hidden) the shelf.
  shelf->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_layout_manager->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_layout_manager->auto_hide_state());
  shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(!shelf_bounds.IsEmpty());

  // Move |widget| so it still overlaps the shelf.
  widget->SetBounds(gfx::Rect(0,
                              shelf_bounds.y() - kWindowHeight + kOverlapSize,
                              kWindowWidth, kWindowHeight));
  widget_bounds = widget->GetWindowBoundsInScreen();
  EXPECT_TRUE(widget_bounds.Intersects(shelf_bounds));
  {
    // Create a mouse-event targeting the top of the shelf widget. This time,
    // window-target should find the shelf as the target.
    gfx::Point event_location(widget_bounds.x() + 5, shelf_bounds.y() + 1);
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
    ui::EventTarget* target = targeter->FindTargetForEvent(root, &mouse);
    EXPECT_EQ(shelf_widget->GetNativeWindow(), target);
  }
}

// Tests that the shelf has a slightly larger hit-region for touch-events when
// it's in the auto-hidden state.
TEST_P(ShelfWidgetTest, HiddenShelfHitTestTouch) {
  Shelf* shelf = Shelf::ForPrimaryDisplay();
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
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
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

namespace {

// A TestShelfDelegate that sets the shelf alignment and auto hide behavior in
// OnShelfCreated, to simulate ChromeLauncherController's behavior.
class TestShelfDelegate : public ShelfDelegate {
 public:
  TestShelfDelegate(ShelfAlignment initial_alignment,
                    ShelfAutoHideBehavior initial_auto_hide_behavior)
      : initial_alignment_(initial_alignment),
        initial_auto_hide_behavior_(initial_auto_hide_behavior) {}
  ~TestShelfDelegate() override {}

  // ShelfDelegate implementation.
  void OnShelfCreated(Shelf* shelf) override {
    shelf->SetAlignment(initial_alignment_);
    shelf->SetAutoHideBehavior(initial_auto_hide_behavior_);
  }
  void OnShelfDestroyed(Shelf* shelf) override {}
  void OnShelfAlignmentChanged(Shelf* shelf) override {}
  void OnShelfAutoHideBehaviorChanged(Shelf* shelf) override {}
  void OnShelfAutoHideStateChanged(Shelf* shelf) override {}
  void OnShelfVisibilityStateChanged(Shelf* shelf) override {}
  ShelfID GetShelfIDForAppID(const std::string& app_id) override { return 0; }
  bool HasShelfIDToAppIDMapping(ShelfID id) const override { return false; }
  const std::string& GetAppIDForShelfID(ShelfID id) override {
    return base::EmptyString();
  }
  void PinAppWithID(const std::string& app_id) override {}
  bool IsAppPinned(const std::string& app_id) override { return false; }
  void UnpinAppWithID(const std::string& app_id) override {}

 private:
  ShelfAlignment initial_alignment_;
  ShelfAutoHideBehavior initial_auto_hide_behavior_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfDelegate);
};

// A TestShellDelegate that creates a TestShelfDelegate with initial values.
class ShelfWidgetTestShellDelegate : public test::TestShellDelegate {
 public:
  ShelfWidgetTestShellDelegate() {}
  ~ShelfWidgetTestShellDelegate() override {}

  // test::TestShellDelegate
  ShelfDelegate* CreateShelfDelegate(ShelfModel* model) override {
    return new TestShelfDelegate(initial_alignment_,
                                 initial_auto_hide_behavior_);
  }

  void set_initial_alignment(ShelfAlignment alignment) {
    initial_alignment_ = alignment;
  }

  void set_initial_auto_hide_behavior(ShelfAutoHideBehavior behavior) {
    initial_auto_hide_behavior_ = behavior;
  }

 private:
  ShelfAlignment initial_alignment_ = SHELF_ALIGNMENT_BOTTOM;
  ShelfAutoHideBehavior initial_auto_hide_behavior_ =
      SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
  DISALLOW_COPY_AND_ASSIGN(ShelfWidgetTestShellDelegate);
};

class ShelfWidgetTestWithDelegate : public ShelfWidgetTest {
 public:
  ShelfWidgetTestWithDelegate() { set_start_session(false); }
  ~ShelfWidgetTestWithDelegate() override {}

  // ShelfWidgetTest:
  void SetUp() override {
    shelf_widget_test_shell_delegate_ = new ShelfWidgetTestShellDelegate;
    ash_test_helper()->set_test_shell_delegate(
        shelf_widget_test_shell_delegate_);
    ShelfWidgetTest::SetUp();
  }

  void TestCreateShelfWithInitialValues(
      ShelfAlignment initial_alignment,
      ShelfAutoHideBehavior initial_auto_hide_behavior,
      ShelfVisibilityState expected_shelf_visibility_state,
      ShelfAutoHideState expected_shelf_auto_hide_state) {
    shelf_widget_test_shell_delegate_->set_initial_alignment(initial_alignment);
    shelf_widget_test_shell_delegate_->set_initial_auto_hide_behavior(
        initial_auto_hide_behavior);
    SetUserLoggedIn(true);
    SetSessionStarted(true);

    ShelfWidget* shelf_widget = GetShelfWidget();
    ASSERT_NE(nullptr, shelf_widget);
    Shelf* shelf = shelf_widget->shelf();
    ASSERT_NE(nullptr, shelf);
    ShelfLayoutManager* shelf_layout_manager =
        shelf_widget->shelf_layout_manager();
    ASSERT_NE(nullptr, shelf_layout_manager);

    EXPECT_EQ(initial_alignment, shelf->alignment());
    EXPECT_EQ(initial_auto_hide_behavior, shelf->auto_hide_behavior());
    EXPECT_EQ(expected_shelf_visibility_state,
              shelf_layout_manager->visibility_state());
    EXPECT_EQ(expected_shelf_auto_hide_state,
              shelf_layout_manager->auto_hide_state());
  }

 private:
  ShelfWidgetTestShellDelegate* shelf_widget_test_shell_delegate_;
  DISALLOW_COPY_AND_ASSIGN(ShelfWidgetTestWithDelegate);
};

}  // namespace

INSTANTIATE_TEST_CASE_P(
    /* prefix intentionally left blank due to only one parameterization */,
    ShelfWidgetTestWithDelegate,
    testing::Values(MaterialDesignController::NON_MATERIAL,
                    MaterialDesignController::MATERIAL_NORMAL,
                    MaterialDesignController::MATERIAL_EXPERIMENTAL));

TEST_P(ShelfWidgetTestWithDelegate, CreateAutoHideAlwaysShelf) {
  // The actual auto hide state is shown because there are no open windows.
  TestCreateShelfWithInitialValues(SHELF_ALIGNMENT_BOTTOM,
                                   SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
                                   SHELF_AUTO_HIDE, SHELF_AUTO_HIDE_SHOWN);
}

TEST_P(ShelfWidgetTestWithDelegate, CreateAutoHideNeverShelf) {
  // The auto hide state 'HIDDEN' is returned for any non-auto-hide behavior.
  TestCreateShelfWithInitialValues(SHELF_ALIGNMENT_LEFT,
                                   SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
                                   SHELF_VISIBLE, SHELF_AUTO_HIDE_HIDDEN);
}

TEST_P(ShelfWidgetTestWithDelegate, CreateAutoHideAlwaysHideShelf) {
  // The auto hide state 'HIDDEN' is returned for any non-auto-hide behavior.
  TestCreateShelfWithInitialValues(SHELF_ALIGNMENT_RIGHT,
                                   SHELF_AUTO_HIDE_ALWAYS_HIDDEN, SHELF_HIDDEN,
                                   SHELF_AUTO_HIDE_HIDDEN);
}

TEST_P(ShelfWidgetTestWithDelegate, CreateLockedShelf) {
  // The auto hide state 'HIDDEN' is returned for any non-auto-hide behavior.
  TestCreateShelfWithInitialValues(SHELF_ALIGNMENT_BOTTOM_LOCKED,
                                   SHELF_AUTO_HIDE_BEHAVIOR_NEVER,
                                   SHELF_VISIBLE, SHELF_AUTO_HIDE_HIDDEN);
}

}  // namespace ash
