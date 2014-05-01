// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"

namespace ash {
namespace wm {
namespace {

class AlwaysMaximizeTestState : public WindowState::State {
 public:
  explicit AlwaysMaximizeTestState(WindowStateType initial_state_type)
      : state_type_(initial_state_type) {}
  virtual ~AlwaysMaximizeTestState() {}

  // WindowState::State overrides:
  virtual void OnWMEvent(WindowState* window_state,
                         const WMEvent* event) OVERRIDE {
    // We don't do anything here.
  }
  virtual WindowStateType GetType() const OVERRIDE {
    return state_type_;
  }
  virtual void AttachState(
      WindowState* window_state,
      WindowState::State* previous_state) OVERRIDE {
    // We always maximize.
    if (state_type_ != WINDOW_STATE_TYPE_MAXIMIZED) {
      window_state->Maximize();
      state_type_ = WINDOW_STATE_TYPE_MAXIMIZED;
    }
  }
  virtual void DetachState(WindowState* window_state) OVERRIDE {}

 private:
  WindowStateType state_type_;

  DISALLOW_COPY_AND_ASSIGN(AlwaysMaximizeTestState);
};

}  // namespace

typedef test::AshTestBase WindowStateTest;

// Test that a window gets properly snapped to the display's edges in a
// multi monitor environment.
TEST_F(WindowStateTest, SnapWindowBasic) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("0+0-500x400, 0+500-600x400");
  const gfx::Rect kPrimaryDisplayWorkAreaBounds =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();
  const gfx::Rect kSecondaryDisplayWorkAreaBounds =
      ScreenUtil::GetSecondaryDisplay().work_area();

  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  WindowState* window_state = GetWindowState(window.get());
  const WMEvent snap_left(WM_EVENT_SNAP_LEFT);
  window_state->OnWMEvent(&snap_left);
  gfx::Rect expected = gfx::Rect(
      kPrimaryDisplayWorkAreaBounds.x(),
      kPrimaryDisplayWorkAreaBounds.y(),
      kPrimaryDisplayWorkAreaBounds.width() / 2,
      kPrimaryDisplayWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  const WMEvent snap_right(WM_EVENT_SNAP_RIGHT);
  window_state->OnWMEvent(&snap_right);
  expected.set_x(kPrimaryDisplayWorkAreaBounds.right() - expected.width());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // Move the window to the secondary display.
  window->SetBoundsInScreen(gfx::Rect(600, 0, 100, 100),
                            ScreenUtil::GetSecondaryDisplay());

  window_state->OnWMEvent(&snap_right);
  expected = gfx::Rect(
      kSecondaryDisplayWorkAreaBounds.x() +
          kSecondaryDisplayWorkAreaBounds.width() / 2,
      kSecondaryDisplayWorkAreaBounds.y(),
      kSecondaryDisplayWorkAreaBounds.width() / 2,
      kSecondaryDisplayWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  window_state->OnWMEvent(&snap_left);
  expected.set_x(kSecondaryDisplayWorkAreaBounds.x());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());
}

// Test how the minimum and maximum size specified by the aura::WindowDelegate
// affect snapping.
TEST_F(WindowStateTest, SnapWindowMinimumSize) {
  if (!SupportsHostWindowResize())
    return;

  UpdateDisplay("0+0-600x900");
  const gfx::Rect kWorkAreaBounds =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(0, 100, kWorkAreaBounds.width() - 1, 100)));

  // It should be possible to snap a window with a minimum size.
  delegate.set_minimum_size(gfx::Size(kWorkAreaBounds.width() - 1, 0));
  WindowState* window_state = GetWindowState(window.get());
  EXPECT_TRUE(window_state->CanSnap());
  const WMEvent snap_right(WM_EVENT_SNAP_RIGHT);
  window_state->OnWMEvent(&snap_right);
  gfx::Rect expected = gfx::Rect(kWorkAreaBounds.x() + 1,
                                 kWorkAreaBounds.y(),
                                 kWorkAreaBounds.width() - 1,
                                 kWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // It should not be possible to snap a window with a maximum size.
  delegate.set_minimum_size(gfx::Size());
  delegate.set_maximum_size(gfx::Size(kWorkAreaBounds.width() - 1, INT_MAX));
  EXPECT_FALSE(window_state->CanSnap());
}

// Test that the minimum size specified by aura::WindowDelegate gets respected.
TEST_F(WindowStateTest, TestRespectMinimumSize) {
  if (!SupportsHostWindowResize())
    return;

  UpdateDisplay("0+0-1024x768");

  aura::test::TestWindowDelegate delegate;
  const gfx::Size minimum_size(gfx::Size(500, 300));
  delegate.set_minimum_size(minimum_size);

  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(0, 100, 100, 100)));

  // Check that the window has the correct minimum size.
  EXPECT_EQ(minimum_size.ToString(), window->bounds().size().ToString());

  // Set the size to something bigger - that should work.
  gfx::Rect bigger_bounds(700, 500, 700, 500);
  window->SetBounds(bigger_bounds);
  EXPECT_EQ(bigger_bounds.ToString(), window->bounds().ToString());

  // Set the size to something smaller - that should only resize to the smallest
  // possible size.
  gfx::Rect smaller_bounds(700, 500, 100, 100);
  window->SetBounds(smaller_bounds);
  EXPECT_EQ(minimum_size.ToString(), window->bounds().size().ToString());
}

// Test that the minimum window size specified by aura::WindowDelegate does not
// exceed the screen size.
TEST_F(WindowStateTest, TestIgnoreTooBigMinimumSize) {
  if (!SupportsHostWindowResize())
    return;

  UpdateDisplay("0+0-1024x768");
  const gfx::Size work_area_size =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area().size();
  const gfx::Size illegal_size(1280, 960);
  const gfx::Rect illegal_bounds(gfx::Point(0, 0), illegal_size);

  aura::test::TestWindowDelegate delegate;
  const gfx::Size minimum_size(illegal_size);
  delegate.set_minimum_size(minimum_size);

  // The creation should force the window to respect the screen size.
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, illegal_bounds));
  EXPECT_EQ(work_area_size.ToString(), window->bounds().size().ToString());

  // Trying to set the size to something bigger then the screen size should be
  // ignored.
  window->SetBounds(illegal_bounds);
  EXPECT_EQ(work_area_size.ToString(), window->bounds().size().ToString());

  // Maximizing the window should not allow it to go bigger than that either.
  WindowState* window_state = GetWindowState(window.get());
  window_state->Maximize();
  EXPECT_EQ(work_area_size.ToString(), window->bounds().size().ToString());
}

// Test that setting the bounds of a snapped window keeps its snapped.
TEST_F(WindowStateTest, SnapWindowSetBounds) {
  if (!SupportsHostWindowResize())
    return;

  UpdateDisplay("0+0-900x600");
  const gfx::Rect kWorkAreaBounds =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();

  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  WindowState* window_state = GetWindowState(window.get());
  const WMEvent snap_left(WM_EVENT_SNAP_LEFT);
  window_state->OnWMEvent(&snap_left);
  EXPECT_EQ(WINDOW_STATE_TYPE_LEFT_SNAPPED, window_state->GetStateType());
  gfx::Rect expected = gfx::Rect(kWorkAreaBounds.x(),
                                 kWorkAreaBounds.y(),
                                 kWorkAreaBounds.width() / 2,
                                 kWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // Snapped windows can have any width.
  expected.set_width(500);
  window->SetBounds(gfx::Rect(10, 10, 500, 300));
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());
  EXPECT_EQ(WINDOW_STATE_TYPE_LEFT_SNAPPED, window_state->GetStateType());
}

// Test that snapping left/right preserves the restore bounds.
TEST_F(WindowStateTest, RestoreBounds) {
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  WindowState* window_state = GetWindowState(window.get());

  EXPECT_TRUE(window_state->IsNormalStateType());

  // 1) Start with restored window with restore bounds set.
  gfx::Rect restore_bounds = window->GetBoundsInScreen();
  restore_bounds.set_width(restore_bounds.width() + 1);
  window_state->SetRestoreBoundsInScreen(restore_bounds);
  const WMEvent snap_left(WM_EVENT_SNAP_LEFT);
  window_state->OnWMEvent(&snap_left);
  const WMEvent snap_right(WM_EVENT_SNAP_RIGHT);
  window_state->OnWMEvent(&snap_right);
  EXPECT_NE(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());
  EXPECT_EQ(restore_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());
  window_state->Restore();
  EXPECT_EQ(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());

  // 2) Start with restored bounds set as a result of maximizing the window.
  window_state->Maximize();
  gfx::Rect maximized_bounds = window->GetBoundsInScreen();
  EXPECT_NE(maximized_bounds.ToString(), restore_bounds.ToString());
  EXPECT_EQ(restore_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());

  window_state->OnWMEvent(&snap_left);
  EXPECT_NE(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());
  EXPECT_NE(maximized_bounds.ToString(),
            window->GetBoundsInScreen().ToString());
  EXPECT_EQ(restore_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

// Test that maximizing an auto managed window, then snapping it puts the window
// at the snapped bounds and not at the auto-managed (centered) bounds.
TEST_F(WindowStateTest, AutoManaged) {
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = GetWindowState(window.get());
  window_state->set_window_position_managed(true);
  window->Hide();
  window->SetBounds(gfx::Rect(100, 100, 100, 100));
  window->Show();

  window_state->Maximize();
  const WMEvent snap_right(WM_EVENT_SNAP_RIGHT);
  window_state->OnWMEvent(&snap_right);

  const gfx::Rect kWorkAreaBounds =
      ash::Shell::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect expected_snapped_bounds(
      kWorkAreaBounds.x() + kWorkAreaBounds.width() / 2,
      kWorkAreaBounds.y(),
      kWorkAreaBounds.width() / 2,
      kWorkAreaBounds.height());
  EXPECT_EQ(expected_snapped_bounds.ToString(),
            window->GetBoundsInScreen().ToString());

  // The window should still be auto managed despite being right maximized.
  EXPECT_TRUE(window_state->window_position_managed());
}

// Test that the replacement of a State object works as expected.
TEST_F(WindowStateTest, SimpleStateSwap) {
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = GetWindowState(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  window_state->SetStateObject(
      scoped_ptr<WindowState::State> (new AlwaysMaximizeTestState(
          window_state->GetStateType())));
  EXPECT_TRUE(window_state->IsMaximized());
}

// Test that the replacement of a state object, following a restore with the
// original one restores the window to its original state.
TEST_F(WindowStateTest, StateSwapRestore) {
  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = GetWindowState(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  scoped_ptr<WindowState::State> old(window_state->SetStateObject(
      scoped_ptr<WindowState::State> (new AlwaysMaximizeTestState(
          window_state->GetStateType()))).Pass());
  EXPECT_TRUE(window_state->IsMaximized());
  window_state->SetStateObject(old.Pass());
  EXPECT_FALSE(window_state->IsMaximized());
}

// TODO(skuhne): Add more unit test to verify the correctness for the restore
// operation.

}  // namespace wm
}  // namespace ash
