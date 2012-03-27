// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/shelf_layout_manager.h"

#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/env.h"
#include "ui/aura/monitor.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/base/animation/animation_container_element.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {

void StepWidgetLayerAnimatorToEnd(views::Widget* widget) {
  ui::AnimationContainerElement* element =
      static_cast<ui::AnimationContainerElement*>(
      widget->GetNativeView()->layer()->GetAnimator());
  element->Step(base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1));
}

ShelfLayoutManager* GetShelfLayoutManager() {
  aura::Window* window = ash::Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_LauncherContainer);
  return static_cast<ShelfLayoutManager*>(window->layout_manager());
}

}  // namespace

class ShelfLayoutManagerTest : public ash::test::AshTestBase {
 public:
  ShelfLayoutManagerTest() {}

  void SetState(ShelfLayoutManager* shelf,
                ShelfLayoutManager::VisibilityState state) {
    shelf->SetState(state);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManagerTest);
};

// Fails on Mac only.  Need to be implemented.  http://crbug.com/111279.
#if defined(OS_MACOSX)
#define MAYBE_SetVisible FAILS_SetVisible
#else
#define MAYBE_SetVisible SetVisible
#endif
// Makes sure SetVisible updates work area and widget appropriately.
TEST_F(ShelfLayoutManagerTest, MAYBE_SetVisible) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  // Force an initial layout.
  shelf->LayoutShelf();
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());
  const aura::MonitorManager* manager =
      aura::Env::GetInstance()->monitor_manager();
  const aura::Monitor* monitor =
      manager->GetMonitorNearestWindow(Shell::GetRootWindow());
  ASSERT_TRUE(monitor);
  // Bottom inset should be the max of widget heights.
  EXPECT_EQ(shelf->shelf_height(),
            monitor->work_area_insets().bottom());

  // Hide the shelf.
  SetState(shelf, ShelfLayoutManager::HIDDEN);
  // Run the animation to completion.
  StepWidgetLayerAnimatorToEnd(shelf->launcher_widget());
  StepWidgetLayerAnimatorToEnd(shelf->status());
  EXPECT_EQ(ShelfLayoutManager::HIDDEN, shelf->visibility_state());
  EXPECT_EQ(0, monitor->work_area_insets().bottom());

  // Make sure the bounds of the two widgets changed.
  EXPECT_GE(shelf->launcher_widget()->GetNativeView()->bounds().y(),
            gfx::Screen::GetPrimaryMonitorBounds().bottom());
  EXPECT_GE(shelf->status()->GetNativeView()->bounds().y(),
            gfx::Screen::GetPrimaryMonitorBounds().bottom());

  // And show it again.
  SetState(shelf, ShelfLayoutManager::VISIBLE);
  // Run the animation to completion.
  StepWidgetLayerAnimatorToEnd(shelf->launcher_widget());
  StepWidgetLayerAnimatorToEnd(shelf->status());
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());
  EXPECT_EQ(shelf->shelf_height(),
            monitor->work_area_insets().bottom());

  // Make sure the bounds of the two widgets changed.
  gfx::Rect launcher_bounds(
      shelf->launcher_widget()->GetNativeView()->bounds());
  int bottom = gfx::Screen::GetPrimaryMonitorBounds().bottom() -
      shelf->shelf_height();
  EXPECT_EQ(launcher_bounds.y(),
            bottom + (shelf->shelf_height() - launcher_bounds.height()) / 2);
  gfx::Rect status_bounds(shelf->status()->GetNativeView()->bounds());
  EXPECT_EQ(status_bounds.y(),
            bottom + shelf->shelf_height() - status_bounds.height());
}

// Makes sure LayoutShelf invoked while animating cleans things up.
TEST_F(ShelfLayoutManagerTest, LayoutShelfWhileAnimating) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  // Force an initial layout.
  shelf->LayoutShelf();
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());

  const aura::MonitorManager* manager =
      aura::Env::GetInstance()->monitor_manager();
  const aura::Monitor* monitor =
      manager->GetMonitorNearestWindow(Shell::GetRootWindow());

  // Hide the shelf.
  SetState(shelf, ShelfLayoutManager::HIDDEN);
  shelf->LayoutShelf();
  EXPECT_EQ(ShelfLayoutManager::HIDDEN, shelf->visibility_state());
  EXPECT_EQ(0, monitor->work_area_insets().bottom());
  // Make sure the bounds of the two widgets changed.
  EXPECT_GE(shelf->launcher_widget()->GetNativeView()->bounds().y(),
            gfx::Screen::GetPrimaryMonitorBounds().bottom());
  EXPECT_GE(shelf->status()->GetNativeView()->bounds().y(),
            gfx::Screen::GetPrimaryMonitorBounds().bottom());
}

// Makes sure the launcher is initially sized correctly.
TEST_F(ShelfLayoutManagerTest, LauncherInitiallySized) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  ShelfLayoutManager* shelf_layout_manager = GetShelfLayoutManager();
  ASSERT_TRUE(shelf_layout_manager);
  ASSERT_TRUE(shelf_layout_manager->status());
  int status_width =
      shelf_layout_manager->status()->GetWindowScreenBounds().width();
  // Test only makes sense if the status is > 0, which is better be.
  EXPECT_GT(status_width, 0);
  EXPECT_EQ(status_width, launcher->GetStatusWidth());
}

// Makes sure the launcher is sized when the status area changes size.
TEST_F(ShelfLayoutManagerTest, LauncherUpdatedWhenStatusAreaChangesSize) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  ShelfLayoutManager* shelf_layout_manager = GetShelfLayoutManager();
  ASSERT_TRUE(shelf_layout_manager);
  ASSERT_TRUE(shelf_layout_manager->status());
  shelf_layout_manager->status()->SetBounds(gfx::Rect(0, 0, 200, 200));
  EXPECT_EQ(200, launcher->GetStatusWidth());
}

// Verifies when the shell is deleted with a full screen window we don't
// crash. This test is here as originally the crash was in ShelfLayoutManager.
TEST_F(ShelfLayoutManagerTest, DontReferenceLauncherAfterDeletion) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->SetFullscreen(true);
}

// Various assertions around auto-hide.
TEST_F(ShelfLayoutManagerTest, DISABLED_AutoHide) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Maximize();
  widget->Show();
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  aura::RootWindow* root = Shell::GetRootWindow();
  // LayoutShelf() forces the animation to completion, at which point the
  // launcher should go off the screen.
  shelf->LayoutShelf();
  EXPECT_EQ(root->bounds().bottom() - ShelfLayoutManager::kAutoHideHeight,
            shelf->launcher_widget()->GetWindowScreenBounds().y());

  // Move the mouse to the bottom of the screen.
  aura::test::EventGenerator generator(root, root);
  generator.MoveMouseTo(gfx::Point(0, root->bounds().bottom() - 1));

  // Shelf should be shown again.
  SetState(shelf, ShelfLayoutManager::AUTO_HIDE);
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  shelf->LayoutShelf();
  EXPECT_EQ(root->bounds().bottom() - shelf->shelf_height(),
            shelf->launcher_widget()->GetWindowScreenBounds().y());

  // Move mouse back up.
  generator.MoveMouseTo(gfx::Point(0, 0));
  SetState(shelf, ShelfLayoutManager::AUTO_HIDE);
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  shelf->LayoutShelf();
  EXPECT_EQ(root->bounds().bottom() - ShelfLayoutManager::kAutoHideHeight,
            shelf->launcher_widget()->GetWindowScreenBounds().y());
}

// Assertions around the lock screen showing.
TEST_F(ShelfLayoutManagerTest, VisibleWhenLockScreenShowing) {
  // Since ShelfLayoutManager queries for mouse location, move the mouse so
  // it isn't over the shelf.
  aura::test::EventGenerator generator(
      Shell::GetInstance()->GetRootWindow(), gfx::Point());
  generator.MoveMouseTo(0, 0);

  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Maximize();
  widget->Show();
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  aura::RootWindow* root = Shell::GetRootWindow();
  // LayoutShelf() forces the animation to completion, at which point the
  // launcher should go off the screen.
  shelf->LayoutShelf();
  EXPECT_EQ(root->bounds().bottom() - ShelfLayoutManager::kAutoHideHeight,
            shelf->launcher_widget()->GetWindowScreenBounds().y());

  aura::Window* lock_container = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_LockScreenContainer);

  views::Widget* lock_widget = new views::Widget;
  views::Widget::InitParams lock_params(
      views::Widget::InitParams::TYPE_WINDOW);
  lock_params.bounds = gfx::Rect(0, 0, 200, 200);
  lock_params.parent = lock_container;
  // Widget is now owned by the parent window.
  lock_widget->Init(lock_params);
  lock_widget->Maximize();
  lock_widget->Show();

  // Lock the screen.
  Shell::GetInstance()->delegate()->LockScreen();
  shelf->UpdateVisibilityState();
  // Showing a widget in the lock screen should force the shelf to be visibile.
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());

  Shell::GetInstance()->delegate()->UnlockScreen();
  shelf->UpdateVisibilityState();
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE, shelf->visibility_state());
}

// Assertions around SetAutoHideBehavior.
TEST_F(ShelfLayoutManagerTest, SetAutoHideBehavior) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Show();
  aura::Window* window = widget->GetNativeWindow();
  gfx::Rect monitor_bounds(gfx::Screen::GetMonitorAreaNearestWindow(window));
  EXPECT_EQ(monitor_bounds.bottom() - ShelfLayoutManager::kAutoHideHeight,
            shelf->GetMaximizedWindowBounds(window).bottom());
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(monitor_bounds.bottom() - ShelfLayoutManager::kAutoHideHeight,
            shelf->GetMaximizedWindowBounds(window).bottom());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT);
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());
  EXPECT_EQ(monitor_bounds.bottom() - ShelfLayoutManager::kAutoHideHeight,
            shelf->GetMaximizedWindowBounds(window).bottom());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());
  EXPECT_GT(monitor_bounds.bottom() - ShelfLayoutManager::kAutoHideHeight,
            shelf->GetMaximizedWindowBounds(window).bottom());

  widget->Maximize();
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());
}

}  // namespace internal
}  // namespace ash
