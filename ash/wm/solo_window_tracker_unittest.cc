// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/solo_window_tracker.h"

#include "ash/ash_constants.h"
#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/screen.h"

namespace ash {

namespace {

class WindowRepaintChecker : public aura::WindowObserver {
 public:
  explicit WindowRepaintChecker(aura::Window* window)
      : window_(window),
        is_paint_scheduled_(false) {
    window_->AddObserver(this);
  }

  virtual ~WindowRepaintChecker() {
    if (window_)
      window_->RemoveObserver(this);
  }

  bool IsPaintScheduledAndReset() {
    bool result = is_paint_scheduled_;
    is_paint_scheduled_ = false;
    return result;
  }

 private:
  // aura::WindowObserver overrides:
  virtual void OnWindowPaintScheduled(aura::Window* window,
                                      const gfx::Rect& region) OVERRIDE {
    is_paint_scheduled_ = true;
  }
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {
    DCHECK_EQ(window_, window);
    window_ = NULL;
  }

  aura::Window* window_;
  bool is_paint_scheduled_;

  DISALLOW_COPY_AND_ASSIGN(WindowRepaintChecker);
};

}  // namespace

class SoloWindowTrackerTest : public test::AshTestBase {
 public:
  SoloWindowTrackerTest() {
  }
  virtual ~SoloWindowTrackerTest() {
  }

  // Helpers methods to create test windows in the primary root window.
  aura::Window* CreateWindowInPrimary() {
    aura::Window* window = new aura::Window(NULL);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    window->SetBounds(gfx::Rect(100, 100));
    ParentWindowInPrimaryRootWindow(window);
    return window;
  }
  aura::Window* CreateAlwaysOnTopWindowInPrimary() {
    aura::Window* window = new aura::Window(NULL);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    window->SetBounds(gfx::Rect(100, 100));
    window->SetProperty(aura::client::kAlwaysOnTopKey, true);
    ParentWindowInPrimaryRootWindow(window);
    return window;
  }
  aura::Window* CreatePanelWindowInPrimary() {
    aura::Window* window = new aura::Window(NULL);
    window->SetType(aura::client::WINDOW_TYPE_PANEL);
    window->Init(ui::LAYER_TEXTURED);
    window->SetBounds(gfx::Rect(100, 100));
    ParentWindowInPrimaryRootWindow(window);
    return window;
  }

  // Drag |window| to the dock.
  void DockWindow(aura::Window* window) {
    // Because the tests use windows without delegates,
    // aura::test::EventGenerator cannot be used.
    gfx::Point drag_to =
        ash::ScreenAsh::GetDisplayBoundsInParent(window).top_right();
    scoped_ptr<WindowResizer> resizer(CreateWindowResizer(
        window,
        window->bounds().origin(),
        HTCAPTION,
        aura::client::WINDOW_MOVE_SOURCE_MOUSE));
    resizer->Drag(drag_to, 0);
    resizer->CompleteDrag(0);
    EXPECT_EQ(internal::kShellWindowId_DockedContainer,
              window->parent()->id());
  }

  // Drag |window| out of the dock.
  void UndockWindow(aura::Window* window) {
    gfx::Point drag_to =
        ash::ScreenAsh::GetDisplayWorkAreaBoundsInParent(window).top_right() -
        gfx::Vector2d(10, 0);
    scoped_ptr<WindowResizer> resizer(CreateWindowResizer(
        window,
        window->bounds().origin(),
        HTCAPTION,
        aura::client::WINDOW_MOVE_SOURCE_MOUSE));
    resizer->Drag(drag_to, 0);
    resizer->CompleteDrag(0);
    EXPECT_NE(internal::kShellWindowId_DockedContainer,
              window->parent()->id());
  }

  // Returns the primary display.
  gfx::Display GetPrimaryDisplay() {
    return ash::Shell::GetInstance()->GetScreen()->GetPrimaryDisplay();
  }

  // Returns the secondary display.
  gfx::Display GetSecondaryDisplay() {
    return ScreenAsh::GetSecondaryDisplay();
  }

  // Returns the window which uses the solo header, if any, on the primary
  // display.
  aura::Window* GetWindowWithSoloHeaderInPrimary() {
    return GetWindowWithSoloHeader(Shell::GetPrimaryRootWindow());
  }

  // Returns the window which uses the solo header, if any, in |root|.
  aura::Window* GetWindowWithSoloHeader(aura::Window* root) {
    SoloWindowTracker* solo_window_tracker =
        internal::GetRootWindowController(root)->solo_window_tracker();
    return solo_window_tracker ?
        solo_window_tracker->GetWindowWithSoloHeader() : NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SoloWindowTrackerTest);
};

TEST_F(SoloWindowTrackerTest, Basic) {
  scoped_ptr<aura::Window> w1(CreateWindowInPrimary());
  w1->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());

  // Create a second window.
  scoped_ptr<aura::Window> w2(CreateWindowInPrimary());
  w2->Show();

  // Now there are two windows, so we should not use solo headers.
  EXPECT_EQ(NULL, GetWindowWithSoloHeaderInPrimary());

  // Hide one window.  Solo should be enabled.
  w2->Hide();
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());

  // Show that window.  Solo should be disabled.
  w2->Show();
  EXPECT_EQ(NULL, GetWindowWithSoloHeaderInPrimary());

  // Minimize the first window.  Solo should be enabled.
  wm::GetWindowState(w1.get())->Minimize();
  EXPECT_EQ(w2.get(), GetWindowWithSoloHeaderInPrimary());

  // Close the minimized window.
  w1.reset();
  EXPECT_EQ(w2.get(), GetWindowWithSoloHeaderInPrimary());

  // Open an always-on-top window (which lives in a different container).
  scoped_ptr<aura::Window> w3(CreateAlwaysOnTopWindowInPrimary());
  w3->Show();
  EXPECT_EQ(NULL, GetWindowWithSoloHeaderInPrimary());

  // Close the always-on-top window.
  w3.reset();
  EXPECT_EQ(w2.get(), GetWindowWithSoloHeaderInPrimary());
}

// Test that docked windows never use the solo header and that the presence of a
// docked window prevents all other windows from the using the solo window
// header.
TEST_F(SoloWindowTrackerTest, DockedWindow) {
  if (!switches::UseDockedWindows() || !SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateWindowInPrimary());
  w1->Show();
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());

  DockWindow(w1.get());
  EXPECT_EQ(NULL, GetWindowWithSoloHeaderInPrimary());

  UndockWindow(w1.get());
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());

  scoped_ptr<aura::Window> w2(CreateWindowInPrimary());
  w2->Show();
  EXPECT_EQ(NULL, GetWindowWithSoloHeaderInPrimary());

  DockWindow(w2.get());
  EXPECT_EQ(NULL, GetWindowWithSoloHeaderInPrimary());

  wm::GetWindowState(w2.get())->Minimize();
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());
}

// Panels should not "count" for computing solo window headers, and the panel
// itself should never use the solo header.
TEST_F(SoloWindowTrackerTest, Panel) {
  scoped_ptr<aura::Window> w1(CreateWindowInPrimary());
  w1->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());

  // Create a panel window.
  scoped_ptr<aura::Window> w2(CreatePanelWindowInPrimary());
  w2->Show();

  // Despite two windows, the first window should still be considered "solo"
  // because panels aren't included in the computation.
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());

  // Even after closing the first window, the panel is still not considered
  // solo.
  w1.reset();
  EXPECT_EQ(NULL, GetWindowWithSoloHeaderInPrimary());
}

// Modal dialogs should not use solo headers.
TEST_F(SoloWindowTrackerTest, Modal) {
  scoped_ptr<aura::Window> w1(CreateWindowInPrimary());
  w1->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());

  // Create a fake modal window.
  scoped_ptr<aura::Window> w2(CreateWindowInPrimary());
  w2->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  w2->Show();

  // Despite two windows, the first window should still be considered "solo"
  // because modal windows aren't included in the computation.
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());
}

// Constrained windows should not use solo headers.
TEST_F(SoloWindowTrackerTest, Constrained) {
  scoped_ptr<aura::Window> w1(CreateWindowInPrimary());
  w1->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());

  // Create a fake constrained window.
  scoped_ptr<aura::Window> w2(CreateWindowInPrimary());
  w2->SetProperty(aura::client::kConstrainedWindowKey, true);
  w2->Show();

  // Despite two windows, the first window should still be considered "solo"
  // because constrained windows aren't included in the computation.
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());
}

// Non-drawing windows should not affect the solo computation.
TEST_F(SoloWindowTrackerTest, NotDrawn) {
  aura::Window* w = CreateWindowInPrimary();
  w->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_EQ(w, GetWindowWithSoloHeaderInPrimary());

  // Create non-drawing window similar to DragDropTracker.
  aura::Window* not_drawn = new aura::Window(NULL);
  not_drawn->SetType(aura::client::WINDOW_TYPE_NORMAL);
  not_drawn->Init(ui::LAYER_NOT_DRAWN);
  ParentWindowInPrimaryRootWindow(not_drawn);
  not_drawn->Show();

  // Despite two windows, the first window should still be considered "solo"
  // because non-drawing windows aren't included in the computation.
  EXPECT_EQ(w, GetWindowWithSoloHeaderInPrimary());
}

TEST_F(SoloWindowTrackerTest, MultiDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("1000x600,600x400");

  scoped_ptr<aura::Window> w1(CreateWindowInPrimary());
  w1->SetBoundsInScreen(gfx::Rect(0, 0, 100, 100), GetPrimaryDisplay());
  w1->Show();
  WindowRepaintChecker checker1(w1.get());
  scoped_ptr<aura::Window> w2(CreateWindowInPrimary());
  w2->SetBoundsInScreen(gfx::Rect(0, 0, 100, 100), GetPrimaryDisplay());
  w2->Show();
  WindowRepaintChecker checker2(w2.get());

  // Now there are two windows in the same display, so we should not use solo
  // headers.
  EXPECT_EQ(NULL, GetWindowWithSoloHeaderInPrimary());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());

  // Moves the second window to the secondary display.  Both w1/w2 should be
  // solo.
  w2->SetBoundsInScreen(gfx::Rect(1200, 0, 100, 100),
                        ScreenAsh::GetSecondaryDisplay());
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());
  EXPECT_EQ(w2.get(), GetWindowWithSoloHeader(w2->GetRootWindow()));
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());
  EXPECT_TRUE(checker2.IsPaintScheduledAndReset());

  // Open two more windows in the primary display.
  scoped_ptr<aura::Window> w3(CreateWindowInPrimary());
  w3->SetBoundsInScreen(gfx::Rect(0, 0, 100, 100), GetPrimaryDisplay());
  w3->Show();
  scoped_ptr<aura::Window> w4(CreateWindowInPrimary());
  w4->SetBoundsInScreen(gfx::Rect(0, 0, 100, 100), GetPrimaryDisplay());
  w4->Show();

  // Because the primary display has three windows w1, w3, and w4, they
  // shouldn't be solo.  w2 should be solo.
  EXPECT_EQ(NULL, GetWindowWithSoloHeaderInPrimary());
  EXPECT_EQ(w2.get(), GetWindowWithSoloHeader(w2->GetRootWindow()));
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());

  // Move w4 to the secondary display.  Now w2 shouldn't be solo anymore.
  w4->SetBoundsInScreen(gfx::Rect(1200, 0, 100, 100), GetSecondaryDisplay());
  EXPECT_EQ(NULL, GetWindowWithSoloHeaderInPrimary());
  EXPECT_EQ(NULL, GetWindowWithSoloHeader(w2->GetRootWindow()));
  EXPECT_TRUE(checker2.IsPaintScheduledAndReset());

  // Moves w3 to the secondary display too.  Now w1 should be solo again.
  w3->SetBoundsInScreen(gfx::Rect(1200, 0, 100, 100), GetSecondaryDisplay());
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());
  EXPECT_EQ(NULL, GetWindowWithSoloHeader(w2->GetRootWindow()));
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());

  // Change w3's state to maximize.  Doesn't affect w1.
  wm::GetWindowState(w3.get())->Maximize();
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());
  EXPECT_EQ(NULL, GetWindowWithSoloHeader(w2->GetRootWindow()));

  // Close w3 and w4.
  w3.reset();
  w4.reset();
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());
  EXPECT_EQ(w2.get(), GetWindowWithSoloHeader(w2->GetRootWindow()));
  EXPECT_TRUE(checker2.IsPaintScheduledAndReset());

  // Move w2 back to the primary display.
  w2->SetBoundsInScreen(gfx::Rect(0, 0, 100, 100), GetPrimaryDisplay());
  EXPECT_EQ(w1->GetRootWindow(), w2->GetRootWindow());
  EXPECT_EQ(NULL, GetWindowWithSoloHeaderInPrimary());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());
  EXPECT_TRUE(checker2.IsPaintScheduledAndReset());

  // Close w2.
  w2.reset();
  EXPECT_EQ(w1.get(), GetWindowWithSoloHeaderInPrimary());
  EXPECT_TRUE(checker1.IsPaintScheduledAndReset());
}

TEST_F(SoloWindowTrackerTest, ChildWindowVisibility) {
  aura::Window* w = CreateWindowInPrimary();
  w->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_EQ(w, GetWindowWithSoloHeaderInPrimary());

  // Create a child window. This should not affect the solo-ness of |w1|.
  aura::Window* child = new aura::Window(NULL);
  child->SetType(aura::client::WINDOW_TYPE_CONTROL);
  child->Init(ui::LAYER_TEXTURED);
  child->SetBounds(gfx::Rect(100, 100));
  w->AddChild(child);
  child->Show();
  EXPECT_EQ(w, GetWindowWithSoloHeaderInPrimary());

  // Changing the visibility of |child| should not affect the solo-ness of |w1|.
  child->Hide();
  EXPECT_EQ(w, GetWindowWithSoloHeaderInPrimary());
}

TEST_F(SoloWindowTrackerTest, CreateAndDeleteSingleWindow) {
  // Ensure that creating/deleting a window works well and doesn't cause
  // crashes.  See crbug.com/155634
  scoped_ptr<aura::Window> w(CreateWindowInPrimary());
  w->Show();

  // We only have one window, so it should use a solo header.
  EXPECT_EQ(w.get(), GetWindowWithSoloHeaderInPrimary());

  // Close the window.
  w.reset();
  EXPECT_EQ(NULL, GetWindowWithSoloHeaderInPrimary());

  // Recreate another window again.
  w.reset(CreateWindowInPrimary());
  w->Show();
  EXPECT_EQ(w.get(), GetWindowWithSoloHeaderInPrimary());
}

}  // namespace ash
