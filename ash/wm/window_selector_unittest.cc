// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shell_test_api.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_selector_controller.h"
#include "ash/wm/window_util.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace ash {
namespace internal {

namespace {

class LayerAnimationObserver : public ui::LayerAnimationObserver {
 public:
  LayerAnimationObserver(ui::Layer* layer)
      : layer_(layer), animating_(false), message_loop_running_(false) {
    layer_->GetAnimator()->AddObserver(this);
  }

  virtual ~LayerAnimationObserver() {
    layer_->GetAnimator()->RemoveObserver(this);
  }

  virtual void OnLayerAnimationEnded(
      ui::LayerAnimationSequence* sequence) OVERRIDE {
    AnimationDone();
  }

  virtual void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) OVERRIDE {
    animating_ = true;
  }

  virtual void OnLayerAnimationAborted(
      ui::LayerAnimationSequence* sequence) OVERRIDE {
    AnimationDone();
  }

  void WaitUntilDone() {
    while (animating_) {
      message_loop_running_ = true;
      base::MessageLoop::current()->Run();
      message_loop_running_ = false;
    }
  }

 private:
  void AnimationDone() {
    animating_ = false;
    if (message_loop_running_)
      base::MessageLoop::current()->Quit();
  }

  ui::Layer* layer_;
  bool animating_;
  bool message_loop_running_;

  DISALLOW_COPY_AND_ASSIGN(LayerAnimationObserver);
};

}  // namespace

class WindowSelectorTest : public test::AshTestBase {
 public:
  WindowSelectorTest() {}
  virtual ~WindowSelectorTest() {}

  aura::Window* CreateWindow(const gfx::Rect& bounds) {
    return CreateTestWindowInShellWithDelegate(&wd, -1, bounds);
  }

  bool WindowsOverlapping(aura::Window* window1, aura::Window* window2) {
    gfx::RectF window1_bounds = GetTransformedTargetBounds(window1);
    gfx::RectF window2_bounds = GetTransformedTargetBounds(window2);
    return window1_bounds.Intersects(window2_bounds);
  }

  void ToggleOverview() {
    std::vector<aura::Window*> windows = ash::Shell::GetInstance()->
        mru_window_tracker()->BuildMruWindowList();
    ScopedVector<LayerAnimationObserver> animations;
    for (size_t i = 0; i < windows.size(); ++i) {
      animations.push_back(new LayerAnimationObserver(windows[i]->layer()));
    }
    ash::Shell::GetInstance()->window_selector_controller()->ToggleOverview();
    for (size_t i = 0; i < animations.size(); ++i) {
      animations[i]->WaitUntilDone();
    }
  }

  void Cycle(WindowSelector::Direction direction) {
    if (!IsSelecting()) {
      std::vector<aura::Window*> windows = ash::Shell::GetInstance()->
          mru_window_tracker()->BuildMruWindowList();
      ScopedVector<LayerAnimationObserver> animations;
      for (size_t i = 0; i < windows.size(); ++i)
        animations.push_back(new LayerAnimationObserver(windows[i]->layer()));
      ash::Shell::GetInstance()->window_selector_controller()->
          HandleCycleWindow(direction);
      for (size_t i = 0; i < animations.size(); ++i)
        animations[i]->WaitUntilDone();
    } else {
      ash::Shell::GetInstance()->window_selector_controller()->
          HandleCycleWindow(direction);
    }
  }

  void StopCycling() {
    ash::Shell::GetInstance()->window_selector_controller()->AltKeyReleased();
  }

  gfx::RectF GetTransformedBounds(aura::Window* window) {
    gfx::RectF bounds(window->layer()->bounds());
    window->layer()->transform().TransformRect(&bounds);
    return bounds;
  }

  gfx::RectF GetTransformedTargetBounds(aura::Window* window) {
    gfx::RectF bounds(window->layer()->GetTargetBounds());
    window->layer()->GetTargetTransform().TransformRect(&bounds);
    return bounds;
  }

  void ClickWindow(aura::Window* window) {
    aura::test::EventGenerator event_generator(window->GetRootWindow(), window);
    gfx::RectF target = GetTransformedBounds(window);
    event_generator.ClickLeftButton();
  }

  bool IsSelecting() {
    return ash::Shell::GetInstance()->window_selector_controller()->
        IsSelecting();
  }

 private:
  aura::test::TestWindowDelegate wd;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorTest);
};

// Tests entering overview mode with two windows and selecting one.
TEST_F(WindowSelectorTest, Basic) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  EXPECT_TRUE(WindowsOverlapping(window1.get(), window2.get()));
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // In overview mode the windows should no longer overlap.
  ToggleOverview();
  EXPECT_FALSE(WindowsOverlapping(window1.get(), window2.get()));

  // Clicking window 1 should activate it.
  ClickWindow(window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
}

// Tests entering overview mode with three windows and cycling through them.
TEST_F(WindowSelectorTest, BasicCycle) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  scoped_ptr<aura::Window> window3(CreateWindow(bounds));
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window3.get()));

  Cycle(WindowSelector::FORWARD);
  EXPECT_TRUE(IsSelecting());
  Cycle(WindowSelector::FORWARD);
  StopCycling();
  EXPECT_FALSE(IsSelecting());
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));
}

// Tests that overview mode is exited if the last remaining window is destroyed.
TEST_F(WindowSelectorTest, LastWindowDestroyed) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  ToggleOverview();

  window1.reset();
  window2.reset();
  EXPECT_FALSE(IsSelecting());
}

// Tests that windows remain on the display they are currently on in overview
// mode.
TEST_F(WindowSelectorTest, MultipleDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x400,400x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  scoped_ptr<aura::Window> window1(CreateWindow(gfx::Rect(0, 0, 100, 100)));
  scoped_ptr<aura::Window> window2(CreateWindow(gfx::Rect(0, 0, 100, 100)));
  scoped_ptr<aura::Window> window3(CreateWindow(gfx::Rect(450, 0, 100, 100)));
  scoped_ptr<aura::Window> window4(CreateWindow(gfx::Rect(450, 0, 100, 100)));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[0], window2->GetRootWindow());
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());
  EXPECT_EQ(root_windows[1], window4->GetRootWindow());

  // In overview mode, each window remains in the same root window.
  ToggleOverview();
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[0], window2->GetRootWindow());
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());
  EXPECT_EQ(root_windows[1], window4->GetRootWindow());
  root_windows[0]->bounds().Contains(
      ToEnclosingRect(GetTransformedBounds(window1.get())));
  root_windows[0]->bounds().Contains(
      ToEnclosingRect(GetTransformedBounds(window2.get())));
  root_windows[1]->bounds().Contains(
      ToEnclosingRect(GetTransformedBounds(window3.get())));
  root_windows[1]->bounds().Contains(
      ToEnclosingRect(GetTransformedBounds(window4.get())));
}

}  // namespace internal
}  // namespace ash
