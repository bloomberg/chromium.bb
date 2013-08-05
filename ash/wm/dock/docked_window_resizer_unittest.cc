// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dock/docked_window_resizer.h"

#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_launcher_delegate.h"
#include "ash/wm/dock/docked_window_layout_manager.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

class DockedWindowResizerTest
    : public test::AshTestBase,
      public testing::WithParamInterface<aura::client::WindowType> {
 public:
  DockedWindowResizerTest() : model_(NULL), window_type_(GetParam()) {}
  virtual ~DockedWindowResizerTest() {}

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableStickyEdges);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableDockedWindows);
    AshTestBase::SetUp();
    UpdateDisplay("600x400");
    test::ShellTestApi test_api(Shell::GetInstance());
    model_ = test_api.launcher_model();
  }

  virtual void TearDown() OVERRIDE {
    AshTestBase::TearDown();
  }

 protected:
  enum DockedEdge {
    DOCKED_EDGE_NONE,
    DOCKED_EDGE_LEFT,
    DOCKED_EDGE_RIGHT,
  };

  enum DockedState {
    UNDOCKED,
    DOCKED,
  };

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        &delegate_,
        window_type_,
        0,
        bounds);
    if (window_type_ == aura::client::WINDOW_TYPE_PANEL) {
      test::TestLauncherDelegate* launcher_delegate =
          test::TestLauncherDelegate::instance();
      launcher_delegate->AddLauncherItem(window);
      PanelLayoutManager* manager =
          static_cast<PanelLayoutManager*>(
              Shell::GetContainer(window->GetRootWindow(),
                                  internal::kShellWindowId_PanelContainer)->
                  layout_manager());
      manager->Relayout();
    }
    return window;
  }

  static WindowResizer* CreateSomeWindowResizer(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component) {
    return CreateWindowResizer(
        window,
        point_in_parent,
        window_component,
        aura::client::WINDOW_MOVE_SOURCE_MOUSE).release();
  }

  void DragStart(aura::Window* window) {
    initial_location_in_parent_ = window->bounds().origin();
    resizer_.reset(CreateSomeWindowResizer(window,
                                           initial_location_in_parent_,
                                           HTCAPTION));
    ASSERT_TRUE(resizer_.get());
  }

  void DragStartAtOffsetFromwindowOrigin(aura::Window* window,
                                         int dx,
                                         int dy) {
    initial_location_in_parent_ =
        window->bounds().origin() + gfx::Vector2d(dx, dy);
    resizer_.reset(CreateSomeWindowResizer(window,
                                           initial_location_in_parent_,
                                           HTCAPTION));
    ASSERT_TRUE(resizer_.get());
  }

  void ResizeStartAtOffsetFromwindowOrigin(aura::Window* window,
                                           int dx,
                                           int dy,
                                           int window_component) {
    initial_location_in_parent_ =
        window->bounds().origin() + gfx::Vector2d(dx, dy);
    resizer_.reset(CreateSomeWindowResizer(window,
                                           initial_location_in_parent_,
                                           window_component));
    ASSERT_TRUE(resizer_.get());
  }

  void DragMove(int dx, int dy) {
    resizer_->Drag(initial_location_in_parent_ + gfx::Vector2d(dx, dy), 0);
  }

  void DragEnd() {
    resizer_->CompleteDrag(0);
    resizer_.reset();
  }

  void DragRevert() {
    resizer_->RevertDrag();
    resizer_.reset();
  }

  // Panels are parented by panel container during drags.
  // Docked windows are parented by dock container during drags.
  // All other windows that we are testing here have default container as a
  // parent.
  int CorrectContainerIdDuringDrag(DockedState is_docked) {
    if (window_type_ == aura::client::WINDOW_TYPE_PANEL)
      return internal::kShellWindowId_PanelContainer;
    if (is_docked == DOCKED)
      return internal::kShellWindowId_DockedContainer;
    return internal::kShellWindowId_DefaultContainer;
  }

  // Test dragging the window vertically (to detach if it is a panel) and then
  // horizontally to the edge with an added offset from the edge of |dx|.
  void DragRelativeToEdge(DockedEdge edge,
                          aura::Window* window,
                          int dx) {
    DragVerticallyAndRelativeToEdge(
        edge,
        window,
        dx,
        window_type_ == aura::client::WINDOW_TYPE_PANEL ? -100 : 20);
  }

  void DragToVerticalPositionAndToEdge(DockedEdge edge,
                                       aura::Window* window,
                                       int y) {
    DragToVerticalPositionRelativeToEdge(edge, window, 0, y);
  }

  void DragToVerticalPositionRelativeToEdge(DockedEdge edge,
                                            aura::Window* window,
                                            int dx,
                                            int y) {
    gfx::Rect initial_bounds = window->GetBoundsInScreen();
    DragVerticallyAndRelativeToEdge(edge, window, dx, y - initial_bounds.y());
  }

  // Detach if our window is a panel, then drag it vertically by |dy| and
  // horizontally to the edge with an added offset from the edge of |dx|.
  void DragVerticallyAndRelativeToEdge(DockedEdge edge,
                                       aura::Window* window,
                                       int dx,
                                       int dy) {
    aura::RootWindow* root_window = window->GetRootWindow();
    gfx::Rect initial_bounds = window->GetBoundsInScreen();

    if (window_type_ == aura::client::WINDOW_TYPE_PANEL) {
      ASSERT_NO_FATAL_FAILURE(DragStart(window));
      EXPECT_TRUE(window->GetProperty(kPanelAttachedKey));

      // Drag enough to detach since our tests assume panels to be initially
      // detached.
      DragMove(0, dy);
      EXPECT_EQ(CorrectContainerIdDuringDrag(UNDOCKED), window->parent()->id());
      EXPECT_EQ(initial_bounds.x(), window->GetBoundsInScreen().x());
      EXPECT_EQ(initial_bounds.y() + dy, window->GetBoundsInScreen().y());

      // The panel should be detached when the drag completes.
      DragEnd();

      EXPECT_FALSE(window->GetProperty(kPanelAttachedKey));
      EXPECT_EQ(internal::kShellWindowId_DefaultContainer,
                window->parent()->id());
      EXPECT_EQ(root_window, window->GetRootWindow());
    }

    // avoid snap by clicking away from the border
    ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(window, 25, 5));

    // Drag the window left or right to the edge (or almost to it).
    if (edge == DOCKED_EDGE_LEFT)
      dx += window->GetRootWindow()->bounds().x() - initial_bounds.x();
    else if (edge == DOCKED_EDGE_RIGHT)
      dx += window->GetRootWindow()->bounds().right() - initial_bounds.right();
    DragMove(dx, window_type_ == aura::client::WINDOW_TYPE_PANEL ? 0 : dy);
    EXPECT_EQ(CorrectContainerIdDuringDrag(UNDOCKED), window->parent()->id());
    // Release the mouse and the panel should be attached to the dock.
    DragEnd();

    // x-coordinate can get adjusted by snapping or sticking.
    // y-coordinate could be changed by possible automatic layout if docked.
    if (window->parent()->id() != internal::kShellWindowId_DockedContainer)
      EXPECT_EQ(initial_bounds.y() + dy, window->GetBoundsInScreen().y());
  }

  bool test_panels() const {
    return window_type_ == aura::client::WINDOW_TYPE_PANEL;
  }

 private:
  scoped_ptr<WindowResizer> resizer_;
  LauncherModel* model_;
  aura::client::WindowType window_type_;
  aura::test::TestWindowDelegate delegate_;

  // Location at start of the drag in |window->parent()|'s coordinates.
  gfx::Point initial_location_in_parent_;

  DISALLOW_COPY_AND_ASSIGN(DockedWindowResizerTest);
};

// Verifies a window can be dragged and attached to the dock.
TEST_P(DockedWindowResizerTest, AttachRightPrecise) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be attached and snapped to the right edge.
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());
}

// Verifies a window can be dragged and attached to the dock
// even if we overshoot the screen edge by a few pixels (sticky edge)
TEST_P(DockedWindowResizerTest, AttachRightOvershoot) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), +4);

  // The window should be attached and snapped to the right edge.
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());
}

// Verifies a window can be dragged and then if not quite reaching the screen
// edge it does not get docked to a screen edge and stays in the desktop.
TEST_P(DockedWindowResizerTest, AttachRightUndershoot) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), -1);

  // The window should not be attached to the dock.
  EXPECT_EQ(window->GetRootWindow()->bounds().right() - 1,
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer,
            window->parent()->id());
}

// Verifies a window can be dragged and attached to the dock.
TEST_P(DockedWindowResizerTest, AttachLeftPrecise) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_LEFT, window.get(), 0);

  // The window should be attached and snapped to the left dock.
  EXPECT_EQ(window->GetRootWindow()->bounds().x(),
            window->GetBoundsInScreen().x());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());
}

// Verifies a window can be dragged and attached to the dock
// even if we overshoot the screen edge by a few pixels (sticky edge)
TEST_P(DockedWindowResizerTest, AttachLeftOvershoot) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_LEFT, window.get(), -4);

  // The window should be attached and snapped to the left dock.
  EXPECT_EQ(window->GetRootWindow()->bounds().x(),
            window->GetBoundsInScreen().x());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());
}

// Verifies a window can be dragged and then if not quite reaching the screen
// edge it does not get docked to a screen edge and stays in the desktop.
TEST_P(DockedWindowResizerTest, AttachLeftUndershoot) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_LEFT, window.get(), 1);

  // The window should not be attached to the dock.
  EXPECT_EQ(window->GetRootWindow()->bounds().x() + 1,
            window->GetBoundsInScreen().x());
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer,
            window->parent()->id());
}

// Dock on the right side, change shelf alignment, check that windows move to
// the opposite side.
TEST_P(DockedWindowResizerTest, AttachRightChangeShelf) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be attached and snapped to the right edge.
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());

  // set launcher shelf to be aligned on the right
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAlignment(SHELF_ALIGNMENT_RIGHT,
                           shell->GetPrimaryRootWindow());
  // The window should have moved and get attached to the left dock.
  EXPECT_EQ(window->GetRootWindow()->bounds().x(),
            window->GetBoundsInScreen().x());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());

  // set launcher shelf to be aligned on the left
  shell->SetShelfAlignment(SHELF_ALIGNMENT_LEFT,
                           shell->GetPrimaryRootWindow());
  // The window should have moved and get attached to the right edge.
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());

  // set launcher shelf to be aligned at the bottom
  shell->SetShelfAlignment(SHELF_ALIGNMENT_BOTTOM,
                           shell->GetPrimaryRootWindow());
  // The window should stay in the right edge.
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());
}

// Dock on the right side, try to undock, then drag more to really undock
TEST_P(DockedWindowResizerTest, AttachTryDetach) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be attached and snapped to the right edge.
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());

  // Try to detach by dragging left a bit (should stay docked)
  ASSERT_NO_FATAL_FAILURE(DragStart(window.get()));
  DragMove(-10, -10);
  // Release the mouse and the window should be still attached to the dock.
  DragEnd();

  // The window should be still attached to the right edge.
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());

  // Try to detach by dragging left a bit more (should get undocked)
  ASSERT_NO_FATAL_FAILURE(DragStart(window.get()));
  DragMove(-32, -10);
  // Release the mouse and the window should be no longer attached to the dock.
  DragEnd();

  // The window should be floating on the desktop again.
  EXPECT_EQ(window->GetRootWindow()->bounds().right() - 32,
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer,
            window->parent()->id());
}

// Minimize a docked window, then restore it and check that it is still docked.
TEST_P(DockedWindowResizerTest, AttachMinimizeRestore) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be attached and snapped to the right edge.
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());

  // Minimize the window, it should be hidden.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(window->IsVisible());
  // Restore the window; window should be visible.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(window->IsVisible());
}

// Dock two windows, undock one, check that the other one is still docked.
TEST_P(DockedWindowResizerTest, AttachTwoWindows)
{
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 50);

  // Both windows should be attached and snapped to the right edge.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());

  EXPECT_EQ(w2->GetRootWindow()->bounds().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());

  // Detach by dragging left (should get undocked).
  ASSERT_NO_FATAL_FAILURE(DragStart(w2.get()));
  // Drag up as well to avoid attaching panels to launcher shelf.
  DragMove(-32, -100);
  // Release the mouse and the window should be no longer attached to the edge.
  DragEnd();

  // The first window should be still docked.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());

  // The second window should be floating on the desktop again.
  EXPECT_EQ(w2->GetRootWindow()->bounds().right() - 32,
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer,
            w2->parent()->id());
}

// Dock one window, try to dock another window on the opposite side (should not
// dock).
TEST_P(DockedWindowResizerTest, AttachOnTwoSides)
{
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_LEFT, w2.get(), 50);

  // The first window should be attached and snapped to the right edge.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());

  // The second window should be near the left edge but not snapped.
  EXPECT_EQ(w2->GetRootWindow()->bounds().x(), w2->GetBoundsInScreen().x());
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer, w2->parent()->id());
}

// Reverting drag
TEST_P(DockedWindowResizerTest, RevertDragRestoresAttachment) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be attached and snapped to the right edge.
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());

  // Drag the window out but revert the drag
  ASSERT_NO_FATAL_FAILURE(DragStart(window.get()));
  DragMove(-50, 0);
  DragRevert();
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());

  // Detach window.
  ASSERT_NO_FATAL_FAILURE(DragStart(window.get()));
  DragMove(-50, 0);
  DragEnd();
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer,
            window->parent()->id());
}

// Move a docked window to the second display
TEST_P(DockedWindowResizerTest, DragAcrossDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("800x800,800x800");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  gfx::Rect initial_bounds = window->GetBoundsInScreen();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);
  // The window should be attached and snapped to the right edge.
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());

  // Undock and move to the right - enough to get it peeking at the other screen
  // but not enough to land in the other screen
  ASSERT_NO_FATAL_FAILURE(DragStart(window.get()));
  DragMove(70, 0);
  EXPECT_EQ(CorrectContainerIdDuringDrag(DOCKED), window->parent()->id());
  DragEnd();
  EXPECT_NE(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer,
            window->parent()->id());
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  // Move back left - should dock again.
  ASSERT_NO_FATAL_FAILURE(DragStart(window.get()));
  DragMove(-70, 0);
  EXPECT_EQ(CorrectContainerIdDuringDrag(UNDOCKED), window->parent()->id());
  DragEnd();
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer,
            window->parent()->id());
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  // Undock and move to the right - enough to get the mouse pointer past the
  // edge of the screen and into the second screen. The window should now be
  // in the second screen and not docked.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(
      window.get(),
      window->bounds().width()/2 + 10,
      0));
  DragMove(window->bounds().width()/2 - 5, 0);
  EXPECT_EQ(CorrectContainerIdDuringDrag(DOCKED), window->parent()->id());
  DragEnd();
  EXPECT_NE(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer,
            window->parent()->id());
  EXPECT_EQ(root_windows[1], window->GetRootWindow());

  // Keep dragging it to the right until it docks. The window should now be
  // in the second screen.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(
      window.get(),
      window->bounds().width()/2 + 10,
      0));
  DragMove(window->GetRootWindow()->GetBoundsInScreen().x() -
           window->GetBoundsInScreen().x(),
           0);
  EXPECT_EQ(CorrectContainerIdDuringDrag(UNDOCKED), window->parent()->id());
  DragEnd();
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().x(),
            window->GetBoundsInScreen().x());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
}

// Dock two windows, undock one.
// Test the docked windows area size and default container resizing.
TEST_P(DockedWindowResizerTest, AttachTwoWindowsDetachOne)
{
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 201)));
  // Work area should cover the whole screen.
  EXPECT_EQ(ScreenAsh::GetDisplayBoundsInParent(w2.get()).width(),
            ScreenAsh::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // A window should be attached and snapped to the right edge.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  EXPECT_EQ(w1->bounds().width(), manager->docked_width_);

  DragToVerticalPositionRelativeToEdge(DOCKED_EDGE_RIGHT, w2.get(), 0, 50);
  // Both windows should now be attached and snapped to the right edge.
  EXPECT_EQ(w2->GetRootWindow()->bounds().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());
  // Dock width should be set to a wider window.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  EXPECT_EQ(std::max(w1->bounds().width(), w2->bounds().width()),
            manager->docked_width_);

  // Try to detach by dragging left a bit (should not get undocked).
  // This would normally detach a single docked window but since we have another
  // window and the mouse pointer does not leave the dock area the window
  // should stay docked.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(w2.get(), 60, 0));
  // Drag up as well as left to avoid attaching panels to launcher shelf.
  DragMove(-40, -40);
  // Release the mouse and the window should be still attached to the edge.
  DragEnd();

  // The first window should be still docked.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());

  // The second window should be still docked.
  EXPECT_EQ(w2->GetRootWindow()->bounds().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());

  // Detach by dragging left more (should get undocked).
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(
      w2.get(),
      w2->bounds().width()/2 + 10,
      0));
  // Drag up as well to avoid attaching panels to launcher shelf.
  DragMove(-(w2->bounds().width()/2 + 20), -100);
  // Release the mouse and the window should be no longer attached to the edge.
  DragEnd();

  // The second window should be floating on the desktop again.
  EXPECT_EQ(w2->GetRootWindow()->bounds().right() -
            (w2->bounds().width()/2 + 20),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer, w2->parent()->id());
  // Dock width should be set to remaining single docked window.
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  EXPECT_EQ(w1->bounds().width(), manager->docked_width_);
}

// Dock one windows. Maximize other testing desktop resizing.
TEST_P(DockedWindowResizerTest, AttachWindowMaximizeOther)
{
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 201)));
  // Work area should cover the whole screen.
  EXPECT_EQ(ScreenAsh::GetDisplayBoundsInParent(w2.get()).width(),
            ScreenAsh::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // A window should be attached and snapped to the right edge.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  EXPECT_EQ(w1->bounds().width(), manager->docked_width_);

  DragToVerticalPositionRelativeToEdge(DOCKED_EDGE_RIGHT,
                                       w2.get(),
                                       -(w2->bounds().width()/2 + 20),
                                       50);
  // The first window should be still docked.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());

  // The second window should be floating on the desktop.
  EXPECT_EQ(w2->GetRootWindow()->bounds().right() -
            (w2->bounds().width()/2 + 20),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer, w2->parent()->id());
  // Dock width should be set to remaining single docked window.
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  EXPECT_EQ(w1->bounds().width(), manager->docked_width_);
  // Desktop work area should now shrink.
  EXPECT_EQ(ScreenAsh::GetDisplayBoundsInParent(w2.get()).width() -
            manager->docked_width_ - DockedWindowLayoutManager::kMinDockGap,
            ScreenAsh::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  // Maximize the second window - Maximized area should be shrunk.
  const gfx::Rect restored_bounds = w2->bounds();
  wm::MaximizeWindow(w2.get());
  EXPECT_EQ(ScreenAsh::GetDisplayBoundsInParent(w2.get()).width() -
            manager->docked_width_ - DockedWindowLayoutManager::kMinDockGap,
            w2->bounds().width());

  // Detach the first window (this should require very little drag).
  ASSERT_NO_FATAL_FAILURE(DragStart(w1.get()));
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  DragMove(-35, 10);
  // For NORMAL windows alignment is set to "RIGHT" until the drag is completed.
  // For PANEL windows alignment is set to "NONE" when drag starts.
  EXPECT_EQ(test_panels() ? DOCKED_ALIGNMENT_NONE : DOCKED_ALIGNMENT_RIGHT,
            manager->alignment_);
  // Release the mouse and the window should be no longer attached to the edge.
  DragEnd();
  // Dock should get shrunk and desktop should get expanded.
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer, w1->parent()->id());
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer, w2->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_NONE, manager->alignment_);
  EXPECT_EQ(0, manager->docked_width_);
  // The second window should now get resized and take up the whole screen.
  EXPECT_EQ(ScreenAsh::GetDisplayBoundsInParent(w2.get()).width(),
            w2->bounds().width());

  // Dock the first window to the left edge.
  // Click at an offset from origin to prevent snapping.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(w1.get(), 10, 0));
  DragMove(-w1->bounds().x(), 0);
  // Alignment set to "NONE" during the drag of the window when none are docked.
  EXPECT_EQ(DOCKED_ALIGNMENT_NONE, manager->alignment_);
  // Release the mouse and the window should be now attached to the edge.
  DragEnd();
  // Dock should get expanded and desktop should get shrunk.
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, manager->alignment_);
  EXPECT_EQ(w1->bounds().width(), manager->docked_width_);
  // Second window should still be in the desktop.
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer, w2->parent()->id());
  // Maximized window should be shrunk.
  EXPECT_EQ(ScreenAsh::GetDisplayBoundsInParent(w2.get()).width() -
            manager->docked_width_ - DockedWindowLayoutManager::kMinDockGap,
            w2->bounds().width());

  // Unmaximize the second window.
  wm::RestoreWindow(w2.get());
  // Its bounds should get restored.
  EXPECT_EQ(restored_bounds, w2->bounds());
}

// Dock one window. Test the sticky behavior near screen or desktop edge.
TEST_P(DockedWindowResizerTest, AttachOneTestSticky)
{
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 201)));
  // Work area should cover the whole screen.
  EXPECT_EQ(ScreenAsh::GetDisplayBoundsInParent(w2.get()).width(),
            ScreenAsh::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_LEFT, w1.get(), 20);
  // A window should be attached and snapped to the left edge.
  EXPECT_EQ(w1->GetRootWindow()->bounds().x(),
            w1->GetBoundsInScreen().x());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());
  // The first window should be docked.
  EXPECT_EQ(w1->GetRootWindow()->bounds().x(),
            w1->GetBoundsInScreen().x());
  // Dock width should be set to that of a single docked window.
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, manager->alignment_);
  EXPECT_EQ(w1->bounds().width(), manager->docked_width_);

  // Position second window in the desktop just to the right of the docked w1.
  DragToVerticalPositionRelativeToEdge(DOCKED_EDGE_LEFT,
                                       w2.get(),
                                       w1->bounds().right() + 20,
                                       50);
  // The second window should be floating on the desktop.
  EXPECT_EQ(w2->GetRootWindow()->bounds().x() + (w1->bounds().right() + 20),
            w2->GetBoundsInScreen().x());
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer, w2->parent()->id());
  // Dock width should be set to that of a single docked window.
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, manager->alignment_);
  EXPECT_EQ(w1->bounds().width(), manager->docked_width_);

  // Drag w2 almost to the dock, the mouse pointer not quite reaching the dock.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(w2.get(), 10, 0));
  DragMove(1 + manager->docked_width_ - w2->bounds().x(), 0);
  // Alignment set to "LEFT" during the drag because dock has a window in it.
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, manager->alignment_);
  // Release the mouse and the window should not be attached to the edge.
  DragEnd();
  // Dock should still have only one window in it.
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, manager->alignment_);
  EXPECT_EQ(w1->bounds().width(), manager->docked_width_);
  // The second window should still be in the desktop.
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer, w2->parent()->id());

  // Drag w2 by a bit more - it should resist the drag (stuck edges)
  int start_x = w2->bounds().x();
  ASSERT_NO_FATAL_FAILURE(DragStart(w2.get()));
  DragMove(-2, 0);
  // Window should not actually move.
  EXPECT_EQ(start_x, w2->bounds().x());
  // Alignment set to "LEFT" during the drag because dock has a window in it.
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, manager->alignment_);
  // Release the mouse and the window should not be attached to the edge.
  DragEnd();
  // Window should be still where it was before the last drag started.
  EXPECT_EQ(start_x, w2->bounds().x());
  // Dock should still have only one window in it
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, manager->alignment_);
  EXPECT_EQ(w1->bounds().width(), manager->docked_width_);
  // The second window should still be in the desktop
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer, w2->parent()->id());

  // Drag w2 by more than the stuck threshold and drop it into the dock.
  ASSERT_NO_FATAL_FAILURE(DragStart(w2.get()));
  DragMove(-100, 0);
  // Window should actually move.
  EXPECT_NE(start_x, w2->bounds().x());
  // Alignment set to "LEFT" during the drag because dock has a window in it.
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, manager->alignment_);
  // Release the mouse and the window should be attached to the edge.
  DragEnd();
  // Both windows are docked now.
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());
  // Dock should get expanded and desktop should get shrunk.
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, manager->alignment_);
  EXPECT_EQ(std::max(w1->bounds().width(), w2->bounds().width()),
            manager->docked_width_);
  // Desktop work area should now shrink by dock width.
  EXPECT_EQ(ScreenAsh::GetDisplayBoundsInParent(w2.get()).width() -
            manager->docked_width_ - DockedWindowLayoutManager::kMinDockGap,
            ScreenAsh::GetDisplayWorkAreaBoundsInParent(w2.get()).width());
}

// Dock two windows, resize one or both.
// Test the docked windows area size and remaining desktop resizing.
TEST_P(DockedWindowResizerTest, ResizeTwoWindows)
{
  if (!SupportsHostWindowResize())
    return;

  // Wider display to start since panels are limited to half the display width.
  UpdateDisplay("1000x400");
  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 201)));
  // Work area should cover the whole screen.
  EXPECT_EQ(ScreenAsh::GetDisplayBoundsInParent(w2.get()).width(),
            ScreenAsh::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // A window should be attached and snapped to the right edge.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  EXPECT_EQ(w1->bounds().width(), manager->docked_width_);

  DragToVerticalPositionRelativeToEdge(DOCKED_EDGE_RIGHT, w2.get(), 0, 50);
  // Both windows should now be attached and snapped to the right edge.
  EXPECT_EQ(w2->GetRootWindow()->bounds().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());
  // Dock width should be set to a wider window.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  EXPECT_EQ(std::max(w1->bounds().width(), w2->bounds().width()),
            manager->docked_width_);

  // Resize the first window left by a bit and test that the dock expands.
  int previous_width = w1->bounds().width();
  const int kResizeSpan1 = 30;
  ASSERT_NO_FATAL_FAILURE(ResizeStartAtOffsetFromwindowOrigin(w1.get(),
                                                              0,
                                                              20,
                                                              HTLEFT));
  DragMove(-kResizeSpan1, 0);
  // Alignment set to "RIGHT" during the drag because dock has a window in it.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  // Release the mouse and the window should be attached to the edge.
  DragEnd();
  // Dock should still have both windows in it.
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  // w1 is now wider than w2 and the dock should expand and be as wide as w1.
  EXPECT_EQ(previous_width + kResizeSpan1, w1->bounds().width());
  EXPECT_GT(w1->bounds().width(), w2->bounds().width());
  EXPECT_EQ(w1->bounds().width(), manager->docked_width_);
  // Desktop work area should shrink.
  EXPECT_EQ(ScreenAsh::GetDisplayBoundsInParent(w2.get()).width() -
            manager->docked_width_ - DockedWindowLayoutManager::kMinDockGap,
            ScreenAsh::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  // Resize the first window left by more than the dock maximum width.
  // This should cause the window to overhang and the dock to shrink to w2.
  previous_width = w1->bounds().width();
  const int kResizeSpan2 = 250;
  ASSERT_NO_FATAL_FAILURE(ResizeStartAtOffsetFromwindowOrigin(w1.get(),
                                                              0,
                                                              20,
                                                              HTLEFT));
  DragMove(-kResizeSpan2, 0);
  // Alignment set to "RIGHT" during the drag because dock has a window in it.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  // Release the mouse and the window should be attached to the edge.
  DragEnd();
  // Dock should still have both windows in it.
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  // w1 is now wider than the maximum dock width and the dock should shrink to
  // the next widest window (w2).
  EXPECT_EQ(previous_width + kResizeSpan2, w1->bounds().width());
  EXPECT_GT(w1->bounds().width(), w2->bounds().width());
  EXPECT_EQ(w2->bounds().width(), manager->docked_width_);
  // Desktop work area should shrink.
  EXPECT_EQ(ScreenAsh::GetDisplayBoundsInParent(w2.get()).width() -
            manager->docked_width_ - DockedWindowLayoutManager::kMinDockGap,
            ScreenAsh::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  // Resize the first window right to get it completely inside the docked area.
  previous_width = w1->bounds().width();
  const int kResizeSpan3 = 100;
  ASSERT_NO_FATAL_FAILURE(ResizeStartAtOffsetFromwindowOrigin(w1.get(),
                                                              0,
                                                              20,
                                                              HTLEFT));
  DragMove(kResizeSpan3, 0);
  // Alignment set to "RIGHT" during the drag because dock has a window in it.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  // Release the mouse and the window should be attached to the edge.
  DragEnd();
  // Dock should still have both windows in it.
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  // w1 is still wider than w2 so the dock should expand and be as wide as w1.
  EXPECT_EQ(previous_width - kResizeSpan3, w1->bounds().width());
  EXPECT_GT(w1->bounds().width(), w2->bounds().width());
  EXPECT_EQ(w1->bounds().width(), manager->docked_width_);
  // Desktop work area should shrink.
  EXPECT_EQ(ScreenAsh::GetDisplayBoundsInParent(w2.get()).width() -
            manager->docked_width_ - DockedWindowLayoutManager::kMinDockGap,
            ScreenAsh::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  // Resize the first window left to be overhang again.
  previous_width = w1->bounds().width();
  ASSERT_NO_FATAL_FAILURE(ResizeStartAtOffsetFromwindowOrigin(w1.get(),
                                                              0,
                                                              20,
                                                              HTLEFT));
  DragMove(-kResizeSpan3, 0);
  DragEnd();
  EXPECT_EQ(previous_width + kResizeSpan3, w1->bounds().width());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  // Docked area should be as wide as the second window - the first is too wide.
  EXPECT_EQ(w2->bounds().width(), manager->docked_width_);

  // Undock the second window. Docked area should shrink to its minimum size.
  ASSERT_NO_FATAL_FAILURE(DragStart(w2.get()));
  // Drag up as well to avoid attaching panels to launcher shelf.
  DragMove(-40, -100);
  // Alignment set to "RIGHT" since we have another window docked.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  // Release the mouse and the window should be no longer attached to the edge.
  DragEnd();
  EXPECT_EQ(internal::kShellWindowId_DefaultContainer, w2->parent()->id());
  // Dock should get shrunk to minimum size.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  EXPECT_EQ(manager->kMinDockWidth, manager->docked_width_);
  // The first window should be still docked.
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  // Desktop work area should be inset.
  EXPECT_EQ(ScreenAsh::GetDisplayBoundsInParent(w2.get()).width() -
            manager->docked_width_ - DockedWindowLayoutManager::kMinDockGap,
            ScreenAsh::GetDisplayWorkAreaBoundsInParent(w2.get()).width());
}

TEST_P(DockedWindowResizerTest, DragToShelf)
{
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  // Work area should cover the whole screen.
  EXPECT_EQ(ScreenAsh::GetDisplayBoundsInParent(w1.get()).width(),
            ScreenAsh::GetDisplayWorkAreaBoundsInParent(w1.get()).width());

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // A window should be attached and snapped to the right edge.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, manager->alignment_);
  EXPECT_EQ(w1->bounds().width(), manager->docked_width_);

  // Detach and drag down to shelf.
  ASSERT_NO_FATAL_FAILURE(DragStart(w1.get()));
  DragMove(-40, 0);
  // For NORMAL windows alignment is set to "RIGHT" until the drag is completed.
  // For PANEL windows alignment is set to "NONE" when drag starts.
  EXPECT_EQ(test_panels() ? DOCKED_ALIGNMENT_NONE : DOCKED_ALIGNMENT_RIGHT,
            manager->alignment_);
  // Release the mouse and the window should be no longer attached to the edge.
  DragEnd();

  // Drag down almost to shelf. A panel will snap, a regular window won't.
  ShelfWidget* shelf = Launcher::ForPrimaryDisplay()->shelf_widget();
  const int shelf_y = shelf->GetWindowBoundsInScreen().y();
  const int kDistanceFromShelf = 10;
  ASSERT_NO_FATAL_FAILURE(DragStart(w1.get()));
  DragMove(0, -kDistanceFromShelf + shelf_y - w1->bounds().bottom());
  DragEnd();
  if (test_panels()) {
    // The panel should be touching the shelf and attached.
    EXPECT_EQ(shelf_y, w1->bounds().bottom());
    EXPECT_TRUE(w1->GetProperty(kPanelAttachedKey));
  } else {
    // The window should not be touching the shelf.
    EXPECT_EQ(shelf_y - kDistanceFromShelf, w1->bounds().bottom());
  }
}

// Tests run twice - on both panels and normal windows
INSTANTIATE_TEST_CASE_P(NormalOrPanel,
                        DockedWindowResizerTest,
                        testing::Values(aura::client::WINDOW_TYPE_NORMAL,
                                        aura::client::WINDOW_TYPE_PANEL));
}  // namespace internal
}  // namespace ash
