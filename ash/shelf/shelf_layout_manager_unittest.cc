// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_layout_manager.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/ash_switches.h"
#include "ash/display/display_manager.h"
#include "ash/focus_cycler.h"
#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_view.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/base/animation/animation_container_element.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace ash {
namespace internal {

namespace {

void StepWidgetLayerAnimatorToEnd(views::Widget* widget) {
  ui::AnimationContainerElement* element =
      static_cast<ui::AnimationContainerElement*>(
      widget->GetNativeView()->layer()->GetAnimator());
  element->Step(base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1));
}

ShelfWidget* GetShelfWidget() {
  return Shell::GetPrimaryRootWindowController()->shelf();
}

ShelfLayoutManager* GetShelfLayoutManager() {
  return Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
}

SystemTray* GetSystemTray() {
  return Shell::GetPrimaryRootWindowController()->GetSystemTray();
}

class ShelfLayoutObserverTest : public ShelfLayoutManager::Observer {
 public:
  ShelfLayoutObserverTest()
      : changed_auto_hide_state_(false) {
  }

  virtual ~ShelfLayoutObserverTest() {}

  bool changed_auto_hide_state() const { return changed_auto_hide_state_; }

 private:
  virtual void OnAutoHideStateChanged(
      ShelfAutoHideState new_state) OVERRIDE {
    changed_auto_hide_state_ = true;
  }

  bool changed_auto_hide_state_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutObserverTest);
};

// Trivial item implementation that tracks its views for testing.
class TestItem : public SystemTrayItem {
 public:
  TestItem()
      : SystemTrayItem(GetSystemTray()),
        tray_view_(NULL),
        default_view_(NULL),
        detailed_view_(NULL),
        notification_view_(NULL) {}

  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE {
    tray_view_ = new views::View;
    // Add a label so it has non-zero width.
    tray_view_->SetLayoutManager(new views::FillLayout);
    tray_view_->AddChildView(new views::Label(UTF8ToUTF16("Tray")));
    return tray_view_;
  }

  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE {
    default_view_ = new views::View;
    default_view_->SetLayoutManager(new views::FillLayout);
    default_view_->AddChildView(new views::Label(UTF8ToUTF16("Default")));
    return default_view_;
  }

  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE {
    detailed_view_ = new views::View;
    detailed_view_->SetLayoutManager(new views::FillLayout);
    detailed_view_->AddChildView(new views::Label(UTF8ToUTF16("Detailed")));
    return detailed_view_;
  }

  virtual views::View* CreateNotificationView(
      user::LoginStatus status) OVERRIDE {
    notification_view_ = new views::View;
    return notification_view_;
  }

  virtual void DestroyTrayView() OVERRIDE {
    tray_view_ = NULL;
  }

  virtual void DestroyDefaultView() OVERRIDE {
    default_view_ = NULL;
  }

  virtual void DestroyDetailedView() OVERRIDE {
    detailed_view_ = NULL;
  }

  virtual void DestroyNotificationView() OVERRIDE {
    notification_view_ = NULL;
  }

  virtual void UpdateAfterLoginStatusChange(
      user::LoginStatus status) OVERRIDE {}

  views::View* tray_view() const { return tray_view_; }
  views::View* default_view() const { return default_view_; }
  views::View* detailed_view() const { return detailed_view_; }
  views::View* notification_view() const { return notification_view_; }

 private:
  views::View* tray_view_;
  views::View* default_view_;
  views::View* detailed_view_;
  views::View* notification_view_;

  DISALLOW_COPY_AND_ASSIGN(TestItem);
};

}  // namespace

class ShelfLayoutManagerTest : public ash::test::AshTestBase {
 public:
  ShelfLayoutManagerTest() {}

  void SetState(ShelfLayoutManager* shelf,
                ShelfVisibilityState state) {
    shelf->SetState(state);
  }

  void UpdateAutoHideStateNow() {
    GetShelfLayoutManager()->UpdateAutoHideStateNow();
  }

  aura::Window* CreateTestWindow() {
    aura::Window* window = new aura::Window(NULL);
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    SetDefaultParentByPrimaryRootWindow(window);
    return window;
  }

  views::Widget* CreateTestWidgetWithParams(
      const views::Widget::InitParams& params) {
    views::Widget* out = new views::Widget;
    out->Init(params);
    out->Show();
    return out;
  }

  // Create a simple widget attached to the current context (will
  // delete on TearDown).
  views::Widget* CreateTestWidget() {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    params.context = CurrentContext();
    return CreateTestWidgetWithParams(params);
  }

  // Overridden from AshTestBase:
  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableTrayDragging);
    test::AshTestBase::SetUp();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManagerTest);
};

// Fails on Mac only.  Need to be implemented.  http://crbug.com/111279.
#if defined(OS_MACOSX) || defined(OS_WIN)
#define MAYBE_SetVisible DISABLED_SetVisible
#else
#define MAYBE_SetVisible SetVisible
#endif
// Makes sure SetVisible updates work area and widget appropriately.
TEST_F(ShelfLayoutManagerTest, MAYBE_SetVisible) {
  ShelfWidget* shelf = GetShelfWidget();
  ShelfLayoutManager* manager = shelf->shelf_layout_manager();
  // Force an initial layout.
  manager->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, manager->visibility_state());

  gfx::Rect status_bounds(
      shelf->status_area_widget()->GetWindowBoundsInScreen());
  gfx::Rect launcher_bounds(
      shelf->GetWindowBoundsInScreen());
  int shelf_height = manager->GetIdealBounds().height();

  const gfx::Display& display = Shell::GetInstance()->display_manager()->
      GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  ASSERT_NE(-1, display.id());
  // Bottom inset should be the max of widget heights.
  EXPECT_EQ(shelf_height,
            display.bounds().bottom() - display.work_area().bottom());

  // Hide the shelf.
  SetState(manager, SHELF_HIDDEN);
  // Run the animation to completion.
  StepWidgetLayerAnimatorToEnd(shelf);
  StepWidgetLayerAnimatorToEnd(shelf->status_area_widget());
  EXPECT_EQ(SHELF_HIDDEN, manager->visibility_state());
  EXPECT_EQ(0,
            display.bounds().bottom() - display.work_area().bottom());

  // Make sure the bounds of the two widgets changed.
  EXPECT_GE(shelf->GetNativeView()->bounds().y(),
            Shell::GetScreen()->GetPrimaryDisplay().bounds().bottom());
  EXPECT_GE(shelf->status_area_widget()->GetNativeView()->bounds().y(),
            Shell::GetScreen()->GetPrimaryDisplay().bounds().bottom());

  // And show it again.
  SetState(manager, SHELF_VISIBLE);
  // Run the animation to completion.
  StepWidgetLayerAnimatorToEnd(shelf);
  StepWidgetLayerAnimatorToEnd(shelf->status_area_widget());
  EXPECT_EQ(SHELF_VISIBLE, manager->visibility_state());
  EXPECT_EQ(shelf_height,
            display.bounds().bottom() - display.work_area().bottom());

  // Make sure the bounds of the two widgets changed.
  launcher_bounds = shelf->GetNativeView()->bounds();
  int bottom =
      Shell::GetScreen()->GetPrimaryDisplay().bounds().bottom() - shelf_height;
  EXPECT_EQ(launcher_bounds.y(),
            bottom + (manager->GetIdealBounds().height() -
                      launcher_bounds.height()) / 2);
  status_bounds = shelf->status_area_widget()->GetNativeView()->bounds();
  EXPECT_EQ(status_bounds.y(),
            bottom + shelf_height - status_bounds.height());
}

// Makes sure LayoutShelf invoked while animating cleans things up.
TEST_F(ShelfLayoutManagerTest, LayoutShelfWhileAnimating) {
  ShelfWidget* shelf = GetShelfWidget();
  // Force an initial layout.
  shelf->shelf_layout_manager()->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->shelf_layout_manager()->visibility_state());

  const gfx::Display& display = Shell::GetInstance()->display_manager()->
      GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());

  // Hide the shelf.
  SetState(shelf->shelf_layout_manager(), SHELF_HIDDEN);
  shelf->shelf_layout_manager()->LayoutShelf();
  EXPECT_EQ(SHELF_HIDDEN, shelf->shelf_layout_manager()->visibility_state());
  EXPECT_EQ(0, display.bounds().bottom() - display.work_area().bottom());

  // Make sure the bounds of the two widgets changed.
  EXPECT_GE(shelf->GetNativeView()->bounds().y(),
            Shell::GetScreen()->GetPrimaryDisplay().bounds().bottom());
  EXPECT_GE(shelf->status_area_widget()->GetNativeView()->bounds().y(),
            Shell::GetScreen()->GetPrimaryDisplay().bounds().bottom());
}

// Makes sure the launcher is sized when the status area changes size.
TEST_F(ShelfLayoutManagerTest, LauncherUpdatedWhenStatusAreaChangesSize) {
  Launcher* launcher = Launcher::ForPrimaryDisplay();
  ASSERT_TRUE(launcher);
  ShelfWidget* shelf_widget = GetShelfWidget();
  ASSERT_TRUE(shelf_widget);
  ASSERT_TRUE(shelf_widget->status_area_widget());
  shelf_widget->status_area_widget()->SetBounds(
      gfx::Rect(0, 0, 200, 200));
  EXPECT_EQ(200, shelf_widget->GetContentsView()->width() -
            launcher->GetLauncherViewForTest()->width());
}


#if defined(OS_WIN)
// RootWindow and Display can't resize on Windows Ash. http://crbug.com/165962
#define MAYBE_AutoHide DISABLED_AutoHide
#else
#define MAYBE_AutoHide AutoHide
#endif

// Various assertions around auto-hide.
TEST_F(ShelfLayoutManagerTest, MAYBE_AutoHide) {
  aura::RootWindow* root = Shell::GetPrimaryRootWindow();
  aura::test::EventGenerator generator(root, root);
  generator.MoveMouseTo(0, 0);

  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Maximize();
  widget->Show();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // LayoutShelf() forces the animation to completion, at which point the
  // launcher should go off the screen.
  shelf->LayoutShelf();
  EXPECT_EQ(root->bounds().bottom() - ShelfLayoutManager::kAutoHideSize,
            GetShelfWidget()->GetWindowBoundsInScreen().y());
  EXPECT_EQ(root->bounds().bottom() - ShelfLayoutManager::kAutoHideSize,
            Shell::GetScreen()->GetDisplayNearestWindow(
                root).work_area().bottom());

  // Move the mouse to the bottom of the screen.
  generator.MoveMouseTo(0, root->bounds().bottom() - 1);

  // Shelf should be shown again (but it shouldn't have changed the work area).
  SetState(shelf, SHELF_AUTO_HIDE);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  shelf->LayoutShelf();
  EXPECT_EQ(root->bounds().bottom() - shelf->GetIdealBounds().height(),
            GetShelfWidget()->GetWindowBoundsInScreen().y());
  EXPECT_EQ(root->bounds().bottom() - ShelfLayoutManager::kAutoHideSize,
            Shell::GetScreen()->GetDisplayNearestWindow(
                root).work_area().bottom());

  // Move mouse back up.
  generator.MoveMouseTo(0, 0);
  SetState(shelf, SHELF_AUTO_HIDE);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  shelf->LayoutShelf();
  EXPECT_EQ(root->bounds().bottom() - ShelfLayoutManager::kAutoHideSize,
            GetShelfWidget()->GetWindowBoundsInScreen().y());

  // Drag mouse to bottom of screen.
  generator.PressLeftButton();
  generator.MoveMouseTo(0, root->bounds().bottom() - 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  generator.ReleaseLeftButton();
  generator.MoveMouseTo(1, root->bounds().bottom() - 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  generator.PressLeftButton();
  generator.MoveMouseTo(1, root->bounds().bottom() - 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
}

// Assertions around the lock screen showing.
TEST_F(ShelfLayoutManagerTest, VisibleWhenLockScreenShowing) {
  // Since ShelfLayoutManager queries for mouse location, move the mouse so
  // it isn't over the shelf.
  aura::test::EventGenerator generator(
      Shell::GetPrimaryRootWindow(), gfx::Point());
  generator.MoveMouseTo(0, 0);

  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Maximize();
  widget->Show();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  aura::RootWindow* root = Shell::GetPrimaryRootWindow();
  // LayoutShelf() forces the animation to completion, at which point the
  // launcher should go off the screen.
  shelf->LayoutShelf();
  EXPECT_EQ(root->bounds().bottom() - ShelfLayoutManager::kAutoHideSize,
            GetShelfWidget()->GetWindowBoundsInScreen().y());

  aura::Window* lock_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(),
      internal::kShellWindowId_LockScreenContainer);

  views::Widget* lock_widget = new views::Widget;
  views::Widget::InitParams lock_params(
      views::Widget::InitParams::TYPE_WINDOW);
  lock_params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  lock_params.parent = lock_container;
  // Widget is now owned by the parent window.
  lock_widget->Init(lock_params);
  lock_widget->Maximize();
  lock_widget->Show();

  // Lock the screen.
  Shell::GetInstance()->delegate()->LockScreen();
  shelf->UpdateVisibilityState();
  // Showing a widget in the lock screen should force the shelf to be visibile.
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  Shell::GetInstance()->delegate()->UnlockScreen();
  shelf->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
}

// Assertions around SetAutoHideBehavior.
TEST_F(ShelfLayoutManagerTest, SetAutoHideBehavior) {
  // Since ShelfLayoutManager queries for mouse location, move the mouse so
  // it isn't over the shelf.
  aura::test::EventGenerator generator(
      Shell::GetPrimaryRootWindow(), gfx::Point());
  generator.MoveMouseTo(0, 0);

  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Show();
  aura::Window* window = widget->GetNativeWindow();
  gfx::Rect display_bounds(
      Shell::GetScreen()->GetDisplayNearestWindow(window).bounds());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  widget->Maximize();
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_EQ(Shell::GetScreen()->GetDisplayNearestWindow(
                window).work_area().bottom(),
            widget->GetWorkAreaBoundsInScreen().bottom());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(Shell::GetScreen()->GetDisplayNearestWindow(
                window).work_area().bottom(),
            widget->GetWorkAreaBoundsInScreen().bottom());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_EQ(Shell::GetScreen()->GetDisplayNearestWindow(
                window).work_area().bottom(),
            widget->GetWorkAreaBoundsInScreen().bottom());
}

// Basic assertions around the dimming of the shelf.
TEST_F(ShelfLayoutManagerTest, TestDimmingBehavior) {
  // Since ShelfLayoutManager queries for mouse location, move the mouse so
  // it isn't over the shelf.
  aura::test::EventGenerator generator(
      Shell::GetPrimaryRootWindow(), gfx::Point());
  generator.MoveMouseTo(0, 0);

  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->shelf_widget()->DisableDimmingAnimationsForTest();

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Show();
  aura::Window* window = widget->GetNativeWindow();
  gfx::Rect display_bounds(
      Shell::GetScreen()->GetDisplayNearestWindow(window).bounds());

  gfx::Point off_shelf = display_bounds.CenterPoint();
  gfx::Point on_shelf =
      shelf->shelf_widget()->GetWindowBoundsInScreen().CenterPoint();

  // Test there is no dimming object active at this point.
  generator.MoveMouseTo(on_shelf.x(), on_shelf.y());
  EXPECT_EQ(-1, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.MoveMouseTo(off_shelf.x(), off_shelf.y());
  EXPECT_EQ(-1, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // After maximization, the shelf should be visible and the dimmer created.
  widget->Maximize();

  on_shelf = shelf->shelf_widget()->GetWindowBoundsInScreen().CenterPoint();
  EXPECT_LT(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // Moving the mouse off the shelf should dim the bar.
  generator.MoveMouseTo(off_shelf.x(), off_shelf.y());
  EXPECT_LT(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // Adding touch events outside the shelf should still keep the shelf in
  // dimmed state.
  generator.PressTouch();
  generator.MoveTouch(off_shelf);
  EXPECT_LT(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  // Move the touch into the shelf area should undim.
  generator.MoveTouch(on_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.ReleaseTouch();
  // And a release dims again.
  EXPECT_LT(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // Moving the mouse on the shelf should undim the bar.
  generator.MoveMouseTo(on_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // No matter what the touch events do, the shelf should stay undimmed.
  generator.PressTouch();
  generator.MoveTouch(off_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.MoveTouch(on_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.MoveTouch(off_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.MoveTouch(on_shelf);
  generator.ReleaseTouch();

  // After restore, the dimming object should be deleted again.
  widget->Restore();
  EXPECT_EQ(-1, shelf->shelf_widget()->GetDimmingAlphaForTest());
}

// Assertions around the dimming of the shelf in conjunction with menus.
TEST_F(ShelfLayoutManagerTest, TestDimmingBehaviorWithMenus) {
  // Since ShelfLayoutManager queries for mouse location, move the mouse so
  // it isn't over the shelf.
  aura::test::EventGenerator generator(
      Shell::GetPrimaryRootWindow(), gfx::Point());
  generator.MoveMouseTo(0, 0);

  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->shelf_widget()->DisableDimmingAnimationsForTest();

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Show();
  aura::Window* window = widget->GetNativeWindow();
  gfx::Rect display_bounds(
      Shell::GetScreen()->GetDisplayNearestWindow(window).bounds());

  // After maximization, the shelf should be visible and the dimmer created.
  widget->Maximize();

  gfx::Point off_shelf = display_bounds.CenterPoint();
  gfx::Point on_shelf =
      shelf->shelf_widget()->GetWindowBoundsInScreen().CenterPoint();

  // Moving the mouse on the shelf should undim the bar.
  generator.MoveMouseTo(on_shelf.x(), on_shelf.y());
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // Simulate a menu opening.
  shelf->shelf_widget()->ForceUndimming(true);

  // Moving the mouse off the shelf should not dim the bar.
  generator.MoveMouseTo(off_shelf.x(), off_shelf.y());
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // No matter what the touch events do, the shelf should stay undimmed.
  generator.PressTouch();
  generator.MoveTouch(off_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.MoveTouch(on_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.MoveTouch(off_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.ReleaseTouch();
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // "Closing the menu" should now turn off the menu since no event is inside
  // the shelf any longer.
  shelf->shelf_widget()->ForceUndimming(false);
  EXPECT_LT(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // Moving the mouse again on the shelf which should undim the bar again.
  // This time we check that the bar stays undimmed when the mouse remains on
  // the bar and the "menu gets closed".
  generator.MoveMouseTo(on_shelf.x(), on_shelf.y());
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  shelf->shelf_widget()->ForceUndimming(true);
  generator.MoveMouseTo(off_shelf.x(), off_shelf.y());
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.MoveMouseTo(on_shelf.x(), on_shelf.y());
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  shelf->shelf_widget()->ForceUndimming(true);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
}

// Verifies the shelf is visible when status/launcher is focused.
TEST_F(ShelfLayoutManagerTest, VisibleWhenStatusOrLauncherFocused) {
  // Since ShelfLayoutManager queries for mouse location, move the mouse so
  // it isn't over the shelf.
  aura::test::EventGenerator generator(
      Shell::GetPrimaryRootWindow(), gfx::Point());
  generator.MoveMouseTo(0, 0);

  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Show();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Focus the launcher. Have to go through the focus cycler as normal focus
  // requests to it do nothing.
  GetShelfWidget()->GetFocusCycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());

  widget->Activate();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Trying to activate the status should fail, since we only allow activating
  // it when the user is using the keyboard (i.e. through FocusCycler).
  GetShelfWidget()->status_area_widget()->Activate();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  GetShelfWidget()->GetFocusCycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
}

// Makes sure shelf will be visible when app list opens as shelf is in
// SHELF_VISIBLE state,and toggling app list won't change shelf
// visibility state.
TEST_F(ShelfLayoutManagerTest, OpenAppListWithShelfVisibleState) {
  Shell* shell = Shell::GetInstance();
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->LayoutShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

  // Create a normal unmaximized windowm shelf should be visible.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  // Toggle app list to show, and the shelf stays visible.
  shell->ToggleAppList(NULL);
  EXPECT_TRUE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  // Toggle app list to hide, and the shelf stays visible.
  shell->ToggleAppList(NULL);
  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
}

// Makes sure shelf will be shown with SHELF_AUTO_HIDE_SHOWN state
// when app list opens as shelf is in SHELF_AUTO_HIDE state, and
// toggling app list won't change shelf visibility state.
TEST_F(ShelfLayoutManagerTest, OpenAppListWithShelfAutoHideState) {
  Shell* shell = Shell::GetInstance();
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->LayoutShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Create a window and show it in maximized state.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window->Show();
  wm::ActivateWindow(window);

  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());

  // Toggle app list to show.
  shell->ToggleAppList(NULL);
  // The shelf's auto hide state won't be changed until the timer fires, so
  // calling shell->UpdateShelfVisibility() is kind of manually helping it to
  // update the state.
  shell->UpdateShelfVisibility();
  EXPECT_TRUE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());

  // Toggle app list to hide.
  shell->ToggleAppList(NULL);
  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
}

// Makes sure shelf will be hidden when app list opens as shelf is in HIDDEN
// state, and toggling app list won't change shelf visibility state.
TEST_F(ShelfLayoutManagerTest, OpenAppListWithShelfHiddenState) {
  Shell* shell = Shell::GetInstance();
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  // For shelf to be visible, app list is not open in initial state.
  shelf->LayoutShelf();

  // Create a window and make it full screen.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  window->Show();
  wm::ActivateWindow(window);

  // App list and shelf is not shown.
  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());

  // Toggle app list to show.
  shell->ToggleAppList(NULL);
  EXPECT_TRUE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());

  // Toggle app list to hide.
  shell->ToggleAppList(NULL);
  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());
}

#if defined(OS_WIN)
// RootWindow and Display can't resize on Windows Ash. http://crbug.com/165962
#define MAYBE_SetAlignment DISABLED_SetAlignment
#else
#define MAYBE_SetAlignment SetAlignment
#endif

// Tests SHELF_ALIGNMENT_(LEFT, RIGHT, TOP).
TEST_F(ShelfLayoutManagerTest, MAYBE_SetAlignment) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  // Force an initial layout.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  shelf->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  gfx::Rect launcher_bounds(
      GetShelfWidget()->GetWindowBoundsInScreen());
  const internal::DisplayManager* manager =
      Shell::GetInstance()->display_manager();
  gfx::Display display =
      manager->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  ASSERT_NE(-1, display.id());
  EXPECT_EQ(shelf->GetIdealBounds().width(),
            display.GetWorkAreaInsets().left());
  EXPECT_GE(
      launcher_bounds.width(),
      GetShelfWidget()->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, GetSystemTray()->shelf_alignment());
  StatusAreaWidget* status_area_widget = GetShelfWidget()->status_area_widget();
  gfx::Rect status_bounds(status_area_widget->GetWindowBoundsInScreen());
  EXPECT_GE(status_bounds.width(),
            status_area_widget->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(shelf->GetIdealBounds().width(),
            display.GetWorkAreaInsets().left());
  EXPECT_EQ(0, display.GetWorkAreaInsets().top());
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());
  EXPECT_EQ(0, display.GetWorkAreaInsets().right());
  EXPECT_EQ(display.bounds().x(), launcher_bounds.x());
  EXPECT_EQ(display.bounds().y(), launcher_bounds.y());
  EXPECT_EQ(display.bounds().height(), launcher_bounds.height());
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  display = manager->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(ShelfLayoutManager::kAutoHideSize,
      display.GetWorkAreaInsets().left());
  EXPECT_EQ(ShelfLayoutManager::kAutoHideSize, display.work_area().x());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);
  display = manager->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  launcher_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  display = manager->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  ASSERT_NE(-1, display.id());
  EXPECT_EQ(shelf->GetIdealBounds().width(),
            display.GetWorkAreaInsets().right());
  EXPECT_GE(launcher_bounds.width(),
      GetShelfWidget()->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT, GetSystemTray()->shelf_alignment());
  status_bounds = gfx::Rect(status_area_widget->GetWindowBoundsInScreen());
  EXPECT_GE(status_bounds.width(),
            status_area_widget->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(shelf->GetIdealBounds().width(),
            display.GetWorkAreaInsets().right());
  EXPECT_EQ(0, display.GetWorkAreaInsets().top());
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());
  EXPECT_EQ(0, display.GetWorkAreaInsets().left());
  EXPECT_EQ(display.work_area().right(), launcher_bounds.x());
  EXPECT_EQ(display.bounds().y(), launcher_bounds.y());
  EXPECT_EQ(display.bounds().height(), launcher_bounds.height());
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  display = manager->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(ShelfLayoutManager::kAutoHideSize,
      display.GetWorkAreaInsets().right());
  EXPECT_EQ(ShelfLayoutManager::kAutoHideSize,
      display.bounds().right() - display.work_area().right());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  shelf->SetAlignment(SHELF_ALIGNMENT_TOP);
  display = manager->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  launcher_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  display = manager->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  ASSERT_NE(-1, display.id());
  EXPECT_EQ(shelf->GetIdealBounds().height(),
            display.GetWorkAreaInsets().top());
  EXPECT_GE(launcher_bounds.height(),
      GetShelfWidget()->GetContentsView()->GetPreferredSize().height());
  EXPECT_EQ(SHELF_ALIGNMENT_TOP, GetSystemTray()->shelf_alignment());
  status_bounds = gfx::Rect(status_area_widget->GetWindowBoundsInScreen());
  EXPECT_GE(status_bounds.height(),
            status_area_widget->GetContentsView()->GetPreferredSize().height());
  EXPECT_EQ(shelf->GetIdealBounds().height(),
            display.GetWorkAreaInsets().top());
  EXPECT_EQ(0, display.GetWorkAreaInsets().right());
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());
  EXPECT_EQ(0, display.GetWorkAreaInsets().left());
  EXPECT_EQ(display.work_area().y(), launcher_bounds.bottom());
  EXPECT_EQ(display.bounds().x(), launcher_bounds.x());
  EXPECT_EQ(display.bounds().width(), launcher_bounds.width());
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  display = manager->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(ShelfLayoutManager::kAutoHideSize,
      display.GetWorkAreaInsets().top());
  EXPECT_EQ(ShelfLayoutManager::kAutoHideSize,
            display.work_area().y() - display.bounds().y());
}

#if defined(OS_WIN)
// RootWindow and Display can't resize on Windows Ash. http://crbug.com/165962
#define MAYBE_GestureDrag DISABLED_GestureDrag
#else
#define MAYBE_GestureDrag GestureDrag
#endif

TEST_F(ShelfLayoutManagerTest, MAYBE_GestureDrag) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  shelf->LayoutShelf();

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  widget->Init(params);
  widget->Show();
  widget->Maximize();

  aura::Window* window = widget->GetNativeWindow();

  gfx::Rect shelf_shown = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Rect bounds_shelf = window->bounds();
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  // Swipe up on the shelf. This should not change any state.
  gfx::Point start = GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint();
  gfx::Point end(start.x(), start.y() + 100);

  // Swipe down on the shelf to hide it.
  end.set_y(start.y() + 100);
  generator.GestureScrollSequence(start, end,
      base::TimeDelta::FromMilliseconds(10), 1);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_NE(bounds_shelf.ToString(), window->bounds().ToString());
  EXPECT_NE(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  gfx::Rect bounds_noshelf = window->bounds();
  gfx::Rect shelf_hidden = GetShelfWidget()->GetWindowBoundsInScreen();

  // Swipe up to show the shelf.
  generator.GestureScrollSequence(end, start,
      base::TimeDelta::FromMilliseconds(10), 1);
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(bounds_shelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up again. The shelf should hide.
  end.set_y(start.y() - 100);
  generator.GestureScrollSequence(start, end,
      base::TimeDelta::FromMilliseconds(10), 1);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up yet again to show it.
  end.set_y(start.y() + 100);
  generator.GestureScrollSequence(end, start,
      base::TimeDelta::FromMilliseconds(10), 1);

  // Swipe down very little. It shouldn't change any state.
  end.set_y(start.y() + shelf_shown.height() * 3 / 10);
  generator.GestureScrollSequence(start, end,
      base::TimeDelta::FromMilliseconds(100), 1);
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(bounds_shelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe down again to hide.
  end.set_y(start.y() + 100);
  generator.GestureScrollSequence(start, end,
      base::TimeDelta::FromMilliseconds(10), 1);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(bounds_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up yet again to show it.
  end.set_y(start.y() + 100);
  generator.GestureScrollSequence(end, start,
      base::TimeDelta::FromMilliseconds(10), 1);
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Tap on the shelf itself. This should not change anything.
  generator.GestureTapAt(start);
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Now, tap on the desktop region (above the shelf). This should hide the
  // shelf.
  gfx::Point tap = start + gfx::Vector2d(0, -90);
  generator.GestureTapAt(tap);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());

  // Make the window fullscreen.
  widget->SetFullscreen(true);
  gfx::Rect bounds_fullscreen = window->bounds();
  EXPECT_TRUE(widget->IsFullscreen());
  EXPECT_NE(bounds_noshelf.ToString(), bounds_fullscreen.ToString());
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());

  // Swipe-up. This should not change anything.
  generator.GestureScrollSequence(end, start,
      base::TimeDelta::FromMilliseconds(10), 1);
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(bounds_fullscreen.ToString(), window->bounds().ToString());
}

TEST_F(ShelfLayoutManagerTest, WindowVisibilityDisablesAutoHide) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->LayoutShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Create a visible window so auto-hide behavior is enforced
  views::Widget* dummy = CreateTestWidget();

  // Window visible => auto hide behaves normally.
  shelf->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Window minimized => auto hide disabled.
  dummy->Minimize();
  shelf->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());

  // Window closed => auto hide disabled.
  dummy->CloseNow();
  shelf->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());

  // Multiple window test
  views::Widget* window1 = CreateTestWidget();
  views::Widget* window2 = CreateTestWidget();

  // both visible => normal autohide
  shelf->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // either minimzed => normal autohide
  window2->Minimize();
  shelf->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  window2->Restore();
  window1->Minimize();
  shelf->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // both minimzed => disable auto hide
  window2->Minimize();
  shelf->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
}

#if defined(OS_WIN)
// RootWindow and Display can't resize on Windows Ash. http://crbug.com/165962
#define MAYBE_GestureRevealsTrayBubble DISABLED_GestureRevealsTrayBubble
#else
#define MAYBE_GestureRevealsTrayBubble GestureRevealsTrayBubble
#endif

TEST_F(ShelfLayoutManagerTest, MAYBE_GestureRevealsTrayBubble) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->LayoutShelf();

  // Create a visible window so auto-hide behavior is enforced.
  CreateTestWidget();

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  SystemTray* tray = GetSystemTray();

  // First, make sure the shelf is visible.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  EXPECT_FALSE(tray->HasSystemBubble());

  // Now, drag up on the tray to show the bubble.
  gfx::Point start = GetShelfWidget()->status_area_widget()->
      GetWindowBoundsInScreen().CenterPoint();
  gfx::Point end(start.x(), start.y() - 100);
  generator.GestureScrollSequence(start, end,
      base::TimeDelta::FromMilliseconds(10), 1);
  EXPECT_TRUE(tray->HasSystemBubble());
  tray->CloseSystemBubbleForTest();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(tray->HasSystemBubble());

  // Drag again, but only a small amount, and slowly. The bubble should not be
  // visible.
  end.set_y(start.y() - 30);
  generator.GestureScrollSequence(start, end,
      base::TimeDelta::FromMilliseconds(500), 100);
  EXPECT_FALSE(tray->HasSystemBubble());

  // Now, hide the shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Start a drag from the bezel, and drag up to show both the shelf and the
  // tray bubble.
  start.set_y(start.y() + 100);
  end.set_y(start.y() - 400);
  generator.GestureScrollSequence(start, end,
      base::TimeDelta::FromMilliseconds(10), 1);
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_TRUE(tray->HasSystemBubble());
}

TEST_F(ShelfLayoutManagerTest, ShelfFlickerOnTrayActivation) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();

  // Create a visible window so auto-hide behavior is enforced.
  CreateTestWidget();

  // Turn on auto-hide for the shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Show the status menu. That should make the shelf visible again.
  Shell::GetInstance()->accelerator_controller()->PerformAction(
      SHOW_SYSTEM_TRAY_BUBBLE, ui::Accelerator());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  EXPECT_TRUE(GetSystemTray()->HasSystemBubble());

  // Now activate the tray (using the keyboard, instead of using the mouse to
  // make sure the mouse does not alter the auto-hide state in the shelf).
  // This should not trigger any auto-hide state change in the shelf.
  ShelfLayoutObserverTest observer;
  shelf->AddObserver(&observer);

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.PressKey(ui::VKEY_SPACE, 0);
  generator.ReleaseKey(ui::VKEY_SPACE, 0);
  EXPECT_TRUE(GetSystemTray()->HasSystemBubble());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  EXPECT_FALSE(observer.changed_auto_hide_state());

  shelf->RemoveObserver(&observer);
}

TEST_F(ShelfLayoutManagerTest, WorkAreaChangeWorkspace) {
  // Make sure the shelf is always visible.
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  shelf->LayoutShelf();

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  views::Widget* widget_one = CreateTestWidgetWithParams(params);
  widget_one->Maximize();

  views::Widget* widget_two = CreateTestWidgetWithParams(params);
  widget_two->Maximize();
  widget_two->Activate();

  // Both windows are maximized. They should be of the same size.
  EXPECT_EQ(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());

  // Now hide the shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // The active maximized window will get resized to the new work area. However,
  // the inactive window should not get resized.
  EXPECT_NE(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());

  // Activate the first window. Now, both windows should be of the same size
  // again.
  widget_one->Activate();
  EXPECT_EQ(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());

  // Now show the shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

  // The active maximized window will get resized to the new work area. However,
  // the inactive window should not get resized.
  EXPECT_NE(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());

  // Activate the first window. Now, both windows should be of the same size
  // again.
  widget_two->Activate();
  EXPECT_EQ(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());
}

// Confirm that the shelf is dimmed only when content is maximized and
// shelf is not autohidden.
TEST_F(ShelfLayoutManagerTest, Dimming) {
  GetShelfLayoutManager()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  scoped_ptr<aura::Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());

  // Normal window doesn't dim shelf.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  ShelfWidget* shelf = GetShelfWidget();
  EXPECT_FALSE(shelf->GetDimsShelf());

  // Maximized window does.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_TRUE(shelf->GetDimsShelf());

  // Change back to normal stops dimming.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_FALSE(shelf->GetDimsShelf());

  // Changing back to maximized dims again.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_TRUE(shelf->GetDimsShelf());

  // Changing shelf to autohide stops dimming.
  GetShelfLayoutManager()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_FALSE(shelf->GetDimsShelf());
}

// Make sure that the shelf will not hide if the mouse is between a bubble and
// the shelf.
TEST_F(ShelfLayoutManagerTest, BubbleEnlargesShelfMouseHitArea) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  StatusAreaWidget* status_area_widget =
      Shell::GetPrimaryRootWindowController()->shelf()->status_area_widget();
  SystemTray* tray = GetSystemTray();

  // Create a visible window so auto-hide behavior is enforced.
  CreateTestWidget();

  shelf->LayoutShelf();
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  // Make two iterations - first without a message bubble which should make
  // the shelf disappear and then with a message bubble which should keep it
  // visible.
  for (int i = 0; i < 2; i++) {
    // Make sure the shelf is visible and position the mouse over it. Then
    // allow auto hide.
    shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
    EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
    gfx::Point center =
        status_area_widget->GetWindowBoundsInScreen().CenterPoint();
    generator.MoveMouseTo(center.x(), center.y());
    shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
    EXPECT_TRUE(shelf->IsVisible());
    if (!i) {
      // In our first iteration we make sure there is no bubble.
      tray->CloseSystemBubbleForTest();
      EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
    } else {
      // In our second iteration we show a bubble.
      TestItem *item = new TestItem;
      tray->AddTrayItem(item);
      tray->ShowNotificationView(item);
      EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
    }
    // Move the pointer over the edge of the shelf.
    generator.MoveMouseTo(
        center.x(), status_area_widget->GetWindowBoundsInScreen().y() - 8);
    shelf->UpdateVisibilityState();
    if (i) {
      EXPECT_TRUE(shelf->IsVisible());
      EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
    } else {
      EXPECT_FALSE(shelf->IsVisible());
      EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
    }
  }
}

}  // namespace internal
}  // namespace ash
