// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/phantom_window_controller.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Returns true if |window| is non-NULL and is visible.
bool IsVisible(aura::Window* window) {
  return window && window->IsVisible();
}

// Observes |window|'s deletion.
class WindowDeletionObserver : public aura::WindowObserver {
 public:
  WindowDeletionObserver(aura::Window* window) : window_(window) {
    window_->AddObserver(this);
  }

  virtual ~WindowDeletionObserver() {
    if (window_)
      window_->RemoveObserver(this);
  }

  // Returns true if the window has not been deleted yet.
  bool IsWindowAlive() {
    return !!window_;
  }

  // aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    window_->RemoveObserver(this);
    window_ = NULL;
   }

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(WindowDeletionObserver);
};

}  // namespace

class PhantomWindowControllerTest : public ash::test::AshTestBase {
 public:
  PhantomWindowControllerTest() {
  }
  virtual ~PhantomWindowControllerTest() {
  }

  // ash::test::AshTestBase:
  virtual void SetUp() OVERRIDE {
    ash::test::AshTestBase::SetUp();

    window_ = CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 50, 60));
    controller_.reset(new PhantomWindowController(window_));
  }

  void DeleteController() {
    controller_.reset();
  }

  PhantomWindowController* controller() {
    return controller_.get();
  }

  aura::Window* window() { return window_; }

  aura::Window* phantom_window_in_target_root() {
    return controller_->phantom_widget_in_target_root_ ?
        controller_->phantom_widget_in_target_root_->GetNativeView() :
        NULL;
  }

  aura::Window* phantom_window_in_start_root() {
    return controller_->phantom_widget_in_start_root_ ?
        controller_->phantom_widget_in_start_root_->GetNativeView() :
        NULL;
  }

 private:
  aura::Window* window_;
  scoped_ptr<PhantomWindowController> controller_;

  DISALLOW_COPY_AND_ASSIGN(PhantomWindowControllerTest);
};

// Test that two phantom windows are used when animating to bounds at least
// partially in another display when using the old caption button style.
TEST_F(PhantomWindowControllerTest, OldCaptionButtonStyle) {
  if (!SupportsMultipleDisplays())
    return;

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshDisableAlternateFrameCaptionButtonStyle);
  ASSERT_FALSE(switches::UseAlternateFrameCaptionButtonStyle());

  UpdateDisplay("500x400,500x400");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(root_windows[0], window()->GetRootWindow());

  // Phantom preview only in the left screen.
  controller()->Show(gfx::Rect(100, 100, 50, 60));
  EXPECT_TRUE(IsVisible(phantom_window_in_target_root()));
  EXPECT_FALSE(IsVisible(phantom_window_in_start_root()));
  EXPECT_EQ(root_windows[0], phantom_window_in_target_root()->GetRootWindow());

  // Move phantom preview into the right screen. Test that 2 windows got
  // created.
  controller()->Show(gfx::Rect(600, 100, 50, 60));
  EXPECT_TRUE(IsVisible(phantom_window_in_target_root()));
  EXPECT_TRUE(IsVisible(phantom_window_in_start_root()));
  EXPECT_EQ(root_windows[1], phantom_window_in_target_root()->GetRootWindow());
  EXPECT_EQ(root_windows[0], phantom_window_in_start_root()->GetRootWindow());

  // Move phantom preview only in the right screen. Start window should close.
  controller()->Show(gfx::Rect(700, 100, 50, 60));
  EXPECT_TRUE(IsVisible(phantom_window_in_target_root()));
  EXPECT_FALSE(IsVisible(phantom_window_in_start_root()));
  EXPECT_EQ(root_windows[1], phantom_window_in_target_root()->GetRootWindow());

  // Move phantom preview into the left screen. Start window should open.
  controller()->Show(gfx::Rect(100, 100, 50, 60));
  EXPECT_TRUE(IsVisible(phantom_window_in_target_root()));
  EXPECT_TRUE(IsVisible(phantom_window_in_start_root()));
  EXPECT_EQ(root_windows[0], phantom_window_in_target_root()->GetRootWindow());
  EXPECT_EQ(root_windows[1], phantom_window_in_start_root()->GetRootWindow());

  // Move phantom preview while in the left screen. Start window should close.
  controller()->Show(gfx::Rect(200, 100, 50, 60));
  EXPECT_TRUE(IsVisible(phantom_window_in_target_root()));
  EXPECT_FALSE(IsVisible(phantom_window_in_start_root()));
  EXPECT_EQ(root_windows[0], phantom_window_in_target_root()->GetRootWindow());

  // Move phantom preview spanning both screens with most of the preview in the
  // right screen. Two windows are created.
  controller()->Show(gfx::Rect(495, 100, 50, 60));
  EXPECT_TRUE(IsVisible(phantom_window_in_target_root()));
  EXPECT_TRUE(IsVisible(phantom_window_in_start_root()));
  EXPECT_EQ(root_windows[1], phantom_window_in_target_root()->GetRootWindow());
  EXPECT_EQ(root_windows[0], phantom_window_in_start_root()->GetRootWindow());

  // Move phantom preview back into the left screen. Phantom windows should
  // swap.
  controller()->Show(gfx::Rect(200, 100, 50, 60));
  EXPECT_TRUE(IsVisible(phantom_window_in_target_root()));
  EXPECT_TRUE(IsVisible(phantom_window_in_start_root()));
  EXPECT_EQ(root_windows[0], phantom_window_in_target_root()->GetRootWindow());
  EXPECT_EQ(root_windows[1], phantom_window_in_start_root()->GetRootWindow());

  // Destroy phantom controller. Both windows should close.
  WindowDeletionObserver target_deletion_observer(
      phantom_window_in_target_root());
  WindowDeletionObserver start_deletion_observer(
      phantom_window_in_start_root());
  DeleteController();
  EXPECT_FALSE(target_deletion_observer.IsWindowAlive());
  EXPECT_FALSE(start_deletion_observer.IsWindowAlive());
}

}  // namespace ash
