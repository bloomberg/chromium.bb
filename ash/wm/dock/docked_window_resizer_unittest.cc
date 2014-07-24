// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dock/docked_window_resizer.h"

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_shelf_delegate.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/dock/docked_window_layout_manager.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class DockedWindowResizerTest
    : public test::AshTestBase,
      public testing::WithParamInterface<ui::wm::WindowType> {
 public:
  DockedWindowResizerTest() : model_(NULL), window_type_(GetParam()) {}
  virtual ~DockedWindowResizerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    UpdateDisplay("600x400");
    test::ShellTestApi test_api(Shell::GetInstance());
    model_ = test_api.shelf_model();
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

  int ideal_width() const { return DockedWindowLayoutManager::kIdealWidth; }
  int min_dock_gap() const { return DockedWindowLayoutManager::kMinDockGap; }
  int max_width() const { return DockedWindowLayoutManager::kMaxDockWidth; }
  int docked_width(const DockedWindowLayoutManager* layout_manager) const {
    return layout_manager->docked_width_;
  }
  int docked_alignment(const DockedWindowLayoutManager* layout_manager) const {
    return layout_manager->alignment_;
  }
  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        &delegate_,
        window_type_,
        0,
        bounds);
    if (window_type_ == ui::wm::WINDOW_TYPE_PANEL) {
      test::TestShelfDelegate* shelf_delegate =
          test::TestShelfDelegate::instance();
      shelf_delegate->AddShelfItem(window);
      PanelLayoutManager* manager = static_cast<PanelLayoutManager*>(
          Shell::GetContainer(window->GetRootWindow(),
                              kShellWindowId_PanelContainer)->layout_manager());
      manager->Relayout();
    }
    return window;
  }

  aura::Window* CreateModalWindow(const gfx::Rect& bounds) {
    aura::Window* window = new aura::Window(&delegate_);
    window->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_SYSTEM);
    window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    window->Init(aura::WINDOW_LAYER_TEXTURED);
    window->Show();

    if (bounds.IsEmpty()) {
      ParentWindowInPrimaryRootWindow(window);
    } else {
      gfx::Display display =
          Shell::GetScreen()->GetDisplayMatching(bounds);
      aura::Window* root = ash::Shell::GetInstance()->display_controller()->
          GetRootWindowForDisplayId(display.id());
      gfx::Point origin = bounds.origin();
      wm::ConvertPointFromScreen(root, &origin);
      window->SetBounds(gfx::Rect(origin, bounds.size()));
      aura::client::ParentWindowWithContext(window, root, bounds);
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
    DragStartAtOffsetFromWindowOrigin(window, 0, 0);
  }

  void DragStartAtOffsetFromWindowOrigin(aura::Window* window,
                                         int dx, int dy) {
    initial_location_in_parent_ =
        window->bounds().origin() + gfx::Vector2d(dx, dy);
    resizer_.reset(CreateSomeWindowResizer(window,
                                           initial_location_in_parent_,
                                           HTCAPTION));
    ASSERT_TRUE(resizer_.get());
  }

  void ResizeStartAtOffsetFromWindowOrigin(aura::Window* window,
                                           int dx, int dy,
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
    resizer_->CompleteDrag();
    resizer_.reset();
  }

  void DragRevert() {
    resizer_->RevertDrag();
    resizer_.reset();
  }

  // Panels are parented by panel container during drags.
  // All other windows that are tested here are parented by dock container
  // during drags.
  int CorrectContainerIdDuringDrag() {
    if (window_type_ == ui::wm::WINDOW_TYPE_PANEL)
      return kShellWindowId_PanelContainer;
    return kShellWindowId_DockedContainer;
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
        window_type_ == ui::wm::WINDOW_TYPE_PANEL ? -100 : 20,
        25,
        5);
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
    DragVerticallyAndRelativeToEdge(edge,
                                    window,
                                    dx, y - initial_bounds.y(),
                                    25, 5);
  }

  // Detach if our window is a panel, then drag it vertically by |dy| and
  // horizontally to the edge with an added offset from the edge of |dx|.
  void DragVerticallyAndRelativeToEdge(DockedEdge edge,
                                       aura::Window* window,
                                       int dx, int dy,
                                       int grab_x, int grab_y) {
    gfx::Rect initial_bounds = window->GetBoundsInScreen();
    // avoid snap by clicking away from the border
    ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(window,
                                                              grab_x, grab_y));

    gfx::Rect work_area =
        Shell::GetScreen()->GetDisplayNearestWindow(window).work_area();
    gfx::Point initial_location_in_screen = initial_location_in_parent_;
    wm::ConvertPointToScreen(window->parent(), &initial_location_in_screen);
    // Drag the window left or right to the edge (or almost to it).
    if (edge == DOCKED_EDGE_LEFT)
      dx += work_area.x() - initial_location_in_screen.x();
    else if (edge == DOCKED_EDGE_RIGHT)
      dx += work_area.right() - 1 - initial_location_in_screen.x();
    DragMove(dx, dy);
    EXPECT_EQ(CorrectContainerIdDuringDrag(), window->parent()->id());
    // Release the mouse and the panel should be attached to the dock.
    DragEnd();

    // x-coordinate can get adjusted by snapping or sticking.
    // y-coordinate could be changed by possible automatic layout if docked.
    if (window->parent()->id() != kShellWindowId_DockedContainer &&
        !wm::GetWindowState(window)->HasRestoreBounds()) {
      EXPECT_EQ(initial_bounds.y() + dy, window->GetBoundsInScreen().y());
    }
  }

  bool test_panels() const { return window_type_ == ui::wm::WINDOW_TYPE_PANEL; }

  aura::test::TestWindowDelegate* delegate() {
    return &delegate_;
  }

  const gfx::Point& initial_location_in_parent() const {
    return initial_location_in_parent_;
  }

 private:
  scoped_ptr<WindowResizer> resizer_;
  ShelfModel* model_;
  ui::wm::WindowType window_type_;
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

  // The window should be docked at the right edge.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());
}

// Verifies a window can be dragged and attached to the dock
// even if pointer overshoots the screen edge by a few pixels (sticky edge)
TEST_P(DockedWindowResizerTest, AttachRightOvershoot) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), +4);

  // The window should be docked at the right edge.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());
}

// Verifies a window can be dragged and then if a pointer is not quite reaching
// the screen edge the window does not get docked and stays in the desktop.
TEST_P(DockedWindowResizerTest, AttachRightUndershoot) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  // Grabbing at 70px ensures that at least 30% of the window is in screen,
  // otherwise the window would be adjusted in
  // WorkspaceLayoutManager::AdjustWindowBoundsWhenAdded.
  const int kGrabOffsetX = 70;
  const int kUndershootBy = 1;
  DragVerticallyAndRelativeToEdge(DOCKED_EDGE_RIGHT,
                                  window.get(),
                                  -kUndershootBy, test_panels() ? -100 : 20,
                                  kGrabOffsetX, 5);

  // The window right should be past the screen edge but not docked.
  // Initial touch point is 70px to the right which helps to find where the edge
  // should be.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right() +
            window->bounds().width() - kGrabOffsetX - kUndershootBy - 1,
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
}

// Verifies a window can be dragged and attached to the dock.
TEST_P(DockedWindowResizerTest, AttachLeftPrecise) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_LEFT, window.get(), 0);

  // The window should be docked at the left edge.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().x(),
            window->GetBoundsInScreen().x());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());
}

// Verifies a window can be dragged and attached to the dock
// even if pointer overshoots the screen edge by a few pixels (sticky edge)
TEST_P(DockedWindowResizerTest, AttachLeftOvershoot) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_LEFT, window.get(), -4);

  // The window should be docked at the left edge.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().x(),
            window->GetBoundsInScreen().x());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());
}

// Verifies a window can be dragged and then if a pointer is not quite reaching
// the screen edge the window does not get docked and stays in the desktop.
TEST_P(DockedWindowResizerTest, AttachLeftUndershoot) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  gfx::Rect initial_bounds(window->bounds());
  DragRelativeToEdge(DOCKED_EDGE_LEFT, window.get(), 1);

  // The window should be crossing the screen edge but not docked.
  int expected_x = initial_bounds.x() - initial_location_in_parent().x() + 1;
  EXPECT_EQ(expected_x, window->GetBoundsInScreen().x());
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
}

// Dock on the right side, change shelf alignment, check that windows move to
// the opposite side.
TEST_P(DockedWindowResizerTest, AttachRightChangeShelf) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be docked at the right edge.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());

  // set launcher shelf to be aligned on the right
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAlignment(SHELF_ALIGNMENT_RIGHT,
                           shell->GetPrimaryRootWindow());
  // The window should have moved and get attached to the left dock.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().x(),
            window->GetBoundsInScreen().x());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());

  // set launcher shelf to be aligned on the left
  shell->SetShelfAlignment(SHELF_ALIGNMENT_LEFT,
                           shell->GetPrimaryRootWindow());
  // The window should have moved and get attached to the right edge.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());

  // set launcher shelf to be aligned at the bottom
  shell->SetShelfAlignment(SHELF_ALIGNMENT_BOTTOM,
                           shell->GetPrimaryRootWindow());
  // The window should stay in the right edge.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());
}

// Dock on the right side, try to undock, then drag more to really undock
TEST_P(DockedWindowResizerTest, AttachTryDetach) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(
      gfx::Rect(0, 0, ideal_width() + 10, 201)));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be docked at the right edge.
  // Its width should shrink to ideal width.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(ideal_width(), window->GetBoundsInScreen().width());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());

  // Try to detach by dragging left less than kSnapToDockDistance.
  // The window should stay docked.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(
      window.get(), 10, 0));
  DragMove(-4, -10);
  // Release the mouse and the window should be still attached to the dock.
  DragEnd();

  // The window should be still attached to the right edge.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());

  // Try to detach by dragging left by kSnapToDockDistance or more.
  // The window should get undocked.
  const int left_edge = window->bounds().x();
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(
      window.get(), 10, 0));
  DragMove(-32, -10);
  // Release the mouse and the window should be no longer attached to the dock.
  DragEnd();

  // The window should be floating on the desktop again and moved to the left.
  EXPECT_EQ(left_edge - 32, window->GetBoundsInScreen().x());
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
}

// Dock on the right side, and undock by dragging the right edge of the window
// header. This test is useful because both the position of the dragged window
// and the position of the mouse are used in determining whether a window should
// be undocked.
TEST_P(DockedWindowResizerTest, AttachTryDetachDragRightEdgeOfHeader) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(
      gfx::Rect(0, 0, ideal_width() + 10, 201)));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be docked at the right edge.
  // Its width should shrink to ideal width.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(ideal_width(), window->GetBoundsInScreen().width());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());

  // Try to detach by dragging left less than kSnapToDockDistance.
  // The window should stay docked.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(
      window.get(), ideal_width() - 10, 0));
  DragMove(-4, -10);
  // Release the mouse and the window should be still attached to the dock.
  DragEnd();

  // The window should be still attached to the right edge.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());

  // Try to detach by dragging left by kSnapToDockDistance or more.
  // The window should get undocked.
  const int left_edge = window->bounds().x();
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(
      window.get(), ideal_width() - 10, 0));
  DragMove(-32, -10);
  // Release the mouse and the window should be no longer attached to the dock.
  DragEnd();

  // The window should be floating on the desktop again and moved to the left.
  EXPECT_EQ(left_edge - 32, window->GetBoundsInScreen().x());
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
}

// Minimize a docked window, then restore it and check that it is still docked.
TEST_P(DockedWindowResizerTest, AttachMinimizeRestore) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be docked at the right edge.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  // Minimize the window, it should be hidden.
  window_state->Minimize();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(window->IsVisible());
  EXPECT_TRUE(window_state->IsMinimized());
  // Restore the window; window should be visible.
  window_state->Restore();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window_state->IsNormalStateType());
}

// Maximize a docked window and check that it is maximized and no longer docked.
TEST_P(DockedWindowResizerTest, AttachMaximize) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be docked at the right edge.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  // Maximize the window, it should get undocked and maximized in a desktop.
  window_state->Maximize();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
}

// Dock two windows, undock one, check that the other one is still docked.
TEST_P(DockedWindowResizerTest, AttachTwoWindows) {
  if (!SupportsHostWindowResize())
    return;
  UpdateDisplay("600x600");

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 50);

  // Docking second window should not minimize the first.
  wm::WindowState* window_state1 = wm::GetWindowState(w1.get());
  EXPECT_FALSE(window_state1->IsMinimized());

  // Both windows should be docked at the right edge.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());

  EXPECT_EQ(w2->GetRootWindow()->GetBoundsInScreen().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w2->parent()->id());

  // Detach by dragging left (should get undocked).
  const int left_edge = w2->bounds().x();
  ASSERT_NO_FATAL_FAILURE(DragStart(w2.get()));
  // Drag up as well to avoid attaching panels to launcher shelf.
  DragMove(-32, -100);
  // Release the mouse and the window should be no longer attached to the edge.
  DragEnd();

  // The first window should be still docked.
  EXPECT_FALSE(window_state1->IsMinimized());
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());

  // The window should be floating on the desktop again and moved to the left.
  EXPECT_EQ(left_edge - 32, w2->GetBoundsInScreen().x());
  EXPECT_EQ(kShellWindowId_DefaultContainer, w2->parent()->id());
}

// Create two windows, dock one and change shelf to auto-hide.
TEST_P(DockedWindowResizerTest, AttachOneAutoHideShelf) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);

  // w1 should be docked at the right edge.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());

  scoped_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegateAndType(
      NULL, ui::wm::WINDOW_TYPE_NORMAL, 0, gfx::Rect(20, 20, 150, 20)));
  wm::GetWindowState(w2.get())->Maximize();
  EXPECT_EQ(kShellWindowId_DefaultContainer, w2->parent()->id());
  EXPECT_TRUE(wm::GetWindowState(w2.get())->IsMaximized());

  gfx::Rect work_area =
      Shell::GetScreen()->GetDisplayNearestWindow(w1.get()).work_area();
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());

  // Docked window should be centered vertically in the work area.
  EXPECT_EQ(work_area.CenterPoint().y(), w1->bounds().CenterPoint().y());
  // Docked background should extend to the bottom of work area.
  EXPECT_EQ(work_area.bottom(), manager->docked_bounds().bottom());

  // set launcher shelf to be aligned on the right
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
                                  shell->GetPrimaryRootWindow());
  work_area =
        Shell::GetScreen()->GetDisplayNearestWindow(w1.get()).work_area();
  // Docked window should be centered vertically in the work area.
  EXPECT_EQ(work_area.CenterPoint().y(), w1->bounds().CenterPoint().y());
  // Docked background should extend to the bottom of work area.
  EXPECT_EQ(work_area.bottom(), manager->docked_bounds().bottom());
}

// Dock one window, try to dock another window on the opposite side (should not
// dock).
TEST_P(DockedWindowResizerTest, AttachOnTwoSides) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  gfx::Rect initial_bounds(w2->bounds());
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_LEFT, w2.get(), 50);

  // The first window should be docked at the right edge.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());

  // The second window should be near the left edge but not snapped.
  // Normal window will get side-maximized while panels will not.
  int expected_x = test_panels() ?
      (initial_bounds.x() - initial_location_in_parent().x()) : 0;
  EXPECT_EQ(expected_x, w2->GetBoundsInScreen().x());
  EXPECT_EQ(kShellWindowId_DefaultContainer, w2->parent()->id());
}

// Tests that reverting a drag restores docked state if a window was docked.
TEST_P(DockedWindowResizerTest, RevertDragRestoresAttachment) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be docked at the right edge.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());

  // Drag the window out but revert the drag
  ASSERT_NO_FATAL_FAILURE(DragStart(window.get()));
  DragMove(-50, 0);
  DragRevert();
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());

  // Detach window.
  ASSERT_NO_FATAL_FAILURE(DragStart(window.get()));
  DragMove(-50, 0);
  DragEnd();
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
}

// Tests that reverting drag restores undocked state if a window was not docked.
TEST_P(DockedWindowResizerTest, RevertDockedDragRevertsAttachment) {
  if (!SupportsHostWindowResize())
    return;
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  aura::Window* dock_container = Shell::GetContainer(
      window->GetRootWindow(),
      kShellWindowId_DockedContainer);
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(dock_container->layout_manager());
  int previous_container_id = window->parent()->id();
  // Drag the window out but revert the drag
  ASSERT_NO_FATAL_FAILURE(DragStart(window.get()));
  DragMove(-50 - window->bounds().x(), 50 - window->bounds().y());
  EXPECT_EQ(CorrectContainerIdDuringDrag(), window->parent()->id());
  DragRevert();
  EXPECT_EQ(previous_container_id, window->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_NONE, docked_alignment(manager));

  // Drag a window to the left so that it overlaps the screen edge.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(
      window.get(),
      window->bounds().width()/2 + 10,
      0));
  DragMove(-50 - window->bounds().x(), 50 - window->bounds().y());
  DragEnd();
  // The window now overlaps the left screen edge but is not docked.
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_NONE, docked_alignment(manager));
  EXPECT_LT(window->bounds().x(), 0);
  EXPECT_GT(window->bounds().right(), 0);

  // Drag the window further left and revert the drag.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(
      window.get(),
      window->bounds().width()/2 + 10,
      0));
  DragMove(-10, 10);
  DragRevert();
  // The window should be in default container and not docked.
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
  // Docked area alignment should be cleared.
  EXPECT_EQ(DOCKED_ALIGNMENT_NONE, docked_alignment(manager));
}

// Move a docked window to the second display
TEST_P(DockedWindowResizerTest, DragAcrossDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("800x800,800x800");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2, static_cast<int>(root_windows.size()));
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  gfx::Rect initial_bounds = window->GetBoundsInScreen();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);
  // The window should be docked at the right edge.
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());

  // Try dragging to the right - enough to get it peeking at the other screen
  // but not enough to land in the other screen.
  // The window should stay on the left screen.
  ASSERT_NO_FATAL_FAILURE(DragStart(window.get()));
  DragMove(100, 0);
  EXPECT_EQ(CorrectContainerIdDuringDrag(), window->parent()->id());
  DragEnd();
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  // Undock and move to the right - enough to get the mouse pointer past the
  // edge of the screen and into the second screen. The window should now be
  // in the second screen and not docked.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(
      window.get(),
      window->bounds().width()/2 + 10,
      0));
  DragMove(window->bounds().width()/2 - 5, 0);
  EXPECT_EQ(CorrectContainerIdDuringDrag(), window->parent()->id());
  DragEnd();
  EXPECT_NE(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
  EXPECT_EQ(root_windows[1], window->GetRootWindow());

  // Keep dragging it to the right until its left edge touches the screen edge.
  // The window should now be in the second screen and not docked.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(
      window.get(),
      window->bounds().width()/2 + 10,
      0));
  DragMove(window->GetRootWindow()->GetBoundsInScreen().x() -
           window->GetBoundsInScreen().x(),
           0);
  EXPECT_EQ(CorrectContainerIdDuringDrag(), window->parent()->id());
  DragEnd();
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().x(),
            window->GetBoundsInScreen().x());
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
}

// Dock two windows, undock one.
// Test the docked windows area size and default container resizing.
TEST_P(DockedWindowResizerTest, AttachTwoWindowsDetachOne) {
  if (!SupportsHostWindowResize())
    return;
  UpdateDisplay("600x600");

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 201)));
  // Work area should cover the whole screen.
  EXPECT_EQ(ScreenUtil::GetDisplayBoundsInParent(w2.get()).width(),
            ScreenUtil::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // A window should be docked at the right edge.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 100);
  // Both windows should now be docked at the right edge.
  EXPECT_EQ(w2->GetRootWindow()->GetBoundsInScreen().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w2->parent()->id());
  // Dock width should be set to a wider window.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(std::max(w1->bounds().width(), w2->bounds().width()),
            docked_width(manager));

  // Try to detach by dragging left a bit (should not get undocked).
  // This would normally detach a single docked window but since we have another
  // window and the mouse pointer does not leave the dock area the window
  // should stay docked.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(w2.get(), 60, 0));
  // Drag up as well as left to avoid attaching panels to launcher shelf.
  DragMove(-40, -40);
  // Release the mouse and the window should be still attached to the edge.
  DragEnd();

  // The first window should be still docked.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());

  // The second window should be still docked.
  EXPECT_EQ(w2->GetRootWindow()->GetBoundsInScreen().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w2->parent()->id());

  // Detach by dragging left more (should get undocked).
  const int left_edge = w2->bounds().x();
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(
      w2.get(),
      w2->bounds().width()/2 + 10,
      0));
  // Drag up as well to avoid attaching panels to launcher shelf.
  const int drag_x = -(w2->bounds().width()/2 + 20);
  DragMove(drag_x, -100);
  // Release the mouse and the window should be no longer attached to the edge.
  DragEnd();

  // The second window should be floating on the desktop again.
  EXPECT_EQ(left_edge + drag_x, w2->bounds().x());
  EXPECT_EQ(kShellWindowId_DefaultContainer, w2->parent()->id());
  // Dock width should be set to remaining single docked window.
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));
}

// Dock one of the windows. Maximize other testing desktop resizing.
TEST_P(DockedWindowResizerTest, AttachWindowMaximizeOther) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 201)));
  // Work area should cover the whole screen.
  EXPECT_EQ(ScreenUtil::GetDisplayBoundsInParent(w2.get()).width(),
            ScreenUtil::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // A window should be docked at the right edge.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));

  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(w2.get(), 25, 5));
  DragMove(w2->GetRootWindow()->bounds().width()
           -w2->bounds().width()
           -(w2->bounds().width()/2 + 20)
           -w2->bounds().x(),
           50 - w2->bounds().y());
  DragEnd();
  // The first window should be still docked.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());

  // The second window should be floating on the desktop.
  EXPECT_EQ(w2->GetRootWindow()->GetBoundsInScreen().right() -
            (w2->bounds().width()/2 + 20),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DefaultContainer, w2->parent()->id());
  // Dock width should be set to remaining single docked window.
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));
  // Desktop work area should now shrink.
  EXPECT_EQ(ScreenUtil::GetDisplayBoundsInParent(w2.get()).width() -
            docked_width(manager) - min_dock_gap(),
            ScreenUtil::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  // Maximize the second window - Maximized area should be shrunk.
  const gfx::Rect restored_bounds = w2->bounds();
  wm::WindowState* w2_state = wm::GetWindowState(w2.get());
  w2_state->Maximize();
  EXPECT_EQ(ScreenUtil::GetDisplayBoundsInParent(w2.get()).width() -
            docked_width(manager) - min_dock_gap(),
            w2->bounds().width());

  // Detach the first window (this should require very little drag).
  ASSERT_NO_FATAL_FAILURE(DragStart(w1.get()));
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  DragMove(-35, 10);
  // Alignment is set to "NONE" when drag starts.
  EXPECT_EQ(DOCKED_ALIGNMENT_NONE, docked_alignment(manager));
  // Release the mouse and the window should be no longer attached to the edge.
  DragEnd();
  EXPECT_EQ(DOCKED_ALIGNMENT_NONE, docked_alignment(manager));
  // Dock should get shrunk and desktop should get expanded.
  EXPECT_EQ(kShellWindowId_DefaultContainer, w1->parent()->id());
  EXPECT_EQ(kShellWindowId_DefaultContainer, w2->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_NONE, docked_alignment(manager));
  EXPECT_EQ(0, docked_width(manager));
  // The second window should now get resized and take up the whole screen.
  EXPECT_EQ(ScreenUtil::GetDisplayBoundsInParent(w2.get()).width(),
            w2->bounds().width());

  // Dock the first window to the left edge.
  // Click at an offset from origin to prevent snapping.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(w1.get(), 10, 0));
  // Drag left to get pointer touching the screen edge.
  DragMove(-w1->bounds().x() - 10, 0);
  // Alignment set to "NONE" during the drag of the window when none are docked.
  EXPECT_EQ(DOCKED_ALIGNMENT_NONE, docked_alignment(manager));
  // Release the mouse and the window should be now attached to the edge.
  DragEnd();
  // Dock should get expanded and desktop should get shrunk.
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));
  // Second window should still be in the desktop.
  EXPECT_EQ(kShellWindowId_DefaultContainer, w2->parent()->id());
  // Maximized window should be shrunk.
  EXPECT_EQ(ScreenUtil::GetDisplayBoundsInParent(w2.get()).width() -
            docked_width(manager) - min_dock_gap(),
            w2->bounds().width());

  // Unmaximize the second window.
  w2_state->Restore();
  // Its bounds should get restored.
  EXPECT_EQ(restored_bounds, w2->bounds());
}

// Dock one window. Test the sticky behavior near screen or desktop edge.
TEST_P(DockedWindowResizerTest, AttachOneTestSticky) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 201)));
  // Work area should cover the whole screen.
  EXPECT_EQ(ScreenUtil::GetDisplayBoundsInParent(w2.get()).width(),
            ScreenUtil::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_LEFT, w1.get(), 20);
  // A window should be docked at the left edge.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().x(),
            w1->GetBoundsInScreen().x());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());
  // The first window should be docked.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().x(),
            w1->GetBoundsInScreen().x());
  // Dock width should be set to that of a single docked window.
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));

  // Position second window in the desktop 20px to the right of the docked w1.
  DragToVerticalPositionRelativeToEdge(DOCKED_EDGE_LEFT,
                                       w2.get(),
                                       20 + 25 -
                                       min_dock_gap(),
                                       50);
  // The second window should be floating on the desktop.
  EXPECT_EQ(w2->GetRootWindow()->GetBoundsInScreen().x() +
                (w1->bounds().right() + 20),
            w2->GetBoundsInScreen().x());
  EXPECT_EQ(kShellWindowId_DefaultContainer, w2->parent()->id());
  // Dock width should be set to that of a single docked window.
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));

  // Drag w2 almost to the dock, the mouse pointer not quite reaching the dock.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(w2.get(), 10, 0));
  DragMove(1 + docked_width(manager) - w2->bounds().x(), 0);
  // Alignment set to "LEFT" during the drag because dock has a window in it.
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, docked_alignment(manager));
  // Release the mouse and the window should not be attached to the edge.
  DragEnd();
  // Dock should still have only one window in it.
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));
  // The second window should still be in the desktop.
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(kShellWindowId_DefaultContainer, w2->parent()->id());

  // Drag w2 by a bit more - it should resist the drag (stuck edges)
  int start_x = w2->bounds().x();
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromWindowOrigin(w2.get(), 100, 5));
  DragMove(-2, 0);
  // Window should not actually move.
  EXPECT_EQ(start_x, w2->bounds().x());
  // Alignment set to "LEFT" during the drag because dock has a window in it.
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, docked_alignment(manager));
  // Release the mouse and the window should not be attached to the edge.
  DragEnd();
  // Window should be still where it was before the last drag started.
  EXPECT_EQ(start_x, w2->bounds().x());
  // Dock should still have only one window in it
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));
  // The second window should still be in the desktop
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(kShellWindowId_DefaultContainer, w2->parent()->id());

  // Drag w2 by more than the stuck threshold and drop it into the dock.
  ASSERT_NO_FATAL_FAILURE(DragStart(w2.get()));
  DragMove(-100, 0);
  // Window should actually move.
  EXPECT_NE(start_x, w2->bounds().x());
  // Alignment set to "LEFT" during the drag because dock has a window in it.
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, docked_alignment(manager));
  // Release the mouse and the window should be attached to the edge.
  DragEnd();
  // Both windows are docked now.
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(kShellWindowId_DockedContainer, w2->parent()->id());
  // Dock should get expanded and desktop should get shrunk.
  EXPECT_EQ(DOCKED_ALIGNMENT_LEFT, docked_alignment(manager));
  EXPECT_EQ(std::max(w1->bounds().width(), w2->bounds().width()),
            docked_width(manager));
  // Desktop work area should now shrink by dock width.
  EXPECT_EQ(ScreenUtil::GetDisplayBoundsInParent(w2.get()).width() -
            docked_width(manager) - min_dock_gap(),
            ScreenUtil::GetDisplayWorkAreaBoundsInParent(w2.get()).width());
}

// Dock two windows, resize one.
// Test the docked windows area size and remaining desktop resizing.
TEST_P(DockedWindowResizerTest, ResizeOneOfTwoWindows) {
  if (!SupportsHostWindowResize())
    return;

  // Wider display to start since panels are limited to half the display width.
  UpdateDisplay("1000x600");
  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 201)));
  // Work area should cover the whole screen.
  EXPECT_EQ(ScreenUtil::GetDisplayBoundsInParent(w2.get()).width(),
            ScreenUtil::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // A window should be docked at the right edge.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 100);
  // Both windows should now be docked at the right edge.
  EXPECT_EQ(w2->GetRootWindow()->GetBoundsInScreen().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w2->parent()->id());
  // Dock width should be set to a wider window.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(std::max(w1->bounds().width(), w2->bounds().width()),
            docked_width(manager));

  // Resize the first window left by a bit and test that the dock expands.
  int previous_width = w1->bounds().width();
  const int kResizeSpan1 = 30;
  ASSERT_NO_FATAL_FAILURE(ResizeStartAtOffsetFromWindowOrigin(w1.get(),
                                                              0, 20,
                                                              HTLEFT));
  DragMove(-kResizeSpan1, 0);
  // Alignment set to "RIGHT" during the drag because dock has a window in it.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  // Release the mouse and the window should be attached to the edge.
  DragEnd();
  // Dock should still have both windows in it.
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(kShellWindowId_DockedContainer, w2->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  // w1 is now wider than before. The dock should expand and be as wide as w1.
  EXPECT_EQ(previous_width + kResizeSpan1, w1->bounds().width());
  // Both windows should get resized since they both don't have min/max size.
  EXPECT_EQ(w1->bounds().width(), w2->bounds().width());
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));
  // Desktop work area should shrink.
  EXPECT_EQ(ScreenUtil::GetDisplayBoundsInParent(w2.get()).width() -
            docked_width(manager) - min_dock_gap(),
            ScreenUtil::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  // Resize the first window left by more than the dock maximum width.
  // This should cause the window width to be restricted by maximum dock width.
  previous_width = w1->bounds().width();
  const int kResizeSpan2 = 250;
  ASSERT_NO_FATAL_FAILURE(ResizeStartAtOffsetFromWindowOrigin(w1.get(),
                                                              0, 20,
                                                              HTLEFT));
  DragMove(-kResizeSpan2, 0);
  // Alignment set to "RIGHT" during the drag because dock has a window in it.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  // Release the mouse and the window should be attached to the edge.
  DragEnd();
  // Dock should still have both windows in it.
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(kShellWindowId_DockedContainer, w2->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  // w1 is now as wide as the maximum dock width and the dock should get
  // resized to the maximum width.
  EXPECT_EQ(max_width(), w1->bounds().width());
  // Both windows should get resized since they both don't have min/max size.
  EXPECT_EQ(w1->bounds().width(), w2->bounds().width());
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));
  // Desktop work area should shrink.
  EXPECT_EQ(ScreenUtil::GetDisplayBoundsInParent(w2.get()).width() -
            docked_width(manager) - min_dock_gap(),
            ScreenUtil::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  // Resize the first window right to get it completely inside the docked area.
  previous_width = w1->bounds().width();
  const int kResizeSpan3 = 100;
  ASSERT_NO_FATAL_FAILURE(ResizeStartAtOffsetFromWindowOrigin(w1.get(),
                                                              0, 20,
                                                              HTLEFT));
  DragMove(kResizeSpan3, 0);
  // Alignment set to "RIGHT" during the drag because dock has a window in it.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  // Release the mouse and the window should be docked.
  DragEnd();
  // Dock should still have both windows in it.
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(kShellWindowId_DockedContainer, w2->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  // w1 should be narrower than before by the length of the drag.
  EXPECT_EQ(previous_width - kResizeSpan3, w1->bounds().width());
  // Both windows should get resized since they both don't have min/max size.
  EXPECT_EQ(w1->bounds().width(), w2->bounds().width());
  // The dock should be as wide as w1 or w2.
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));
  // Desktop work area should shrink.
  EXPECT_EQ(ScreenUtil::GetDisplayBoundsInParent(w2.get()).width() -
            docked_width(manager) - min_dock_gap(),
            ScreenUtil::GetDisplayWorkAreaBoundsInParent(w2.get()).width());

  // Resize the first window left to be overhang again.
  previous_width = w1->bounds().width();
  ASSERT_NO_FATAL_FAILURE(ResizeStartAtOffsetFromWindowOrigin(w1.get(),
                                                              0, 20,
                                                              HTLEFT));
  DragMove(-kResizeSpan3, 0);
  DragEnd();
  EXPECT_EQ(previous_width + kResizeSpan3, w1->bounds().width());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  // Docked area should be as wide as possible (maximum) and same as w1.
  EXPECT_EQ(max_width(), docked_width(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));

  // Undock the first window. Docked area should shrink to its ideal size.
  ASSERT_NO_FATAL_FAILURE(DragStart(w1.get()));
  // Drag up as well to avoid attaching panels to launcher shelf.
  DragMove(-(400 - 210), -100);
  // Alignment set to "RIGHT" since we have another window docked.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  // Release the mouse and the window should be no longer attached to the edge.
  DragEnd();
  EXPECT_EQ(kShellWindowId_DefaultContainer, w1->parent()->id());
  // Dock should be as wide as w2 (and same as ideal width).
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(ideal_width(), docked_width(manager));
  EXPECT_EQ(w2->bounds().width(), docked_width(manager));
  // The second window should be still docked.
  EXPECT_EQ(kShellWindowId_DockedContainer, w2->parent()->id());
  // Desktop work area should be inset.
  EXPECT_EQ(ScreenUtil::GetDisplayBoundsInParent(w1.get()).width() -
            docked_width(manager) - min_dock_gap(),
            ScreenUtil::GetDisplayWorkAreaBoundsInParent(w1.get()).width());
}

// Dock a window, resize it and test that undocking it preserves the width.
TEST_P(DockedWindowResizerTest, ResizingKeepsWidth) {
  if (!SupportsHostWindowResize())
    return;

  // Wider display to start since panels are limited to half the display width.
  UpdateDisplay("1000x600");
  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // Window should be docked at the right edge.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));

  // Resize the window left by a bit and test that the dock expands.
  int previous_width = w1->bounds().width();
  const int kResizeSpan1 = 30;
  ASSERT_NO_FATAL_FAILURE(ResizeStartAtOffsetFromWindowOrigin(w1.get(),
                                                              0, 20,
                                                              HTLEFT));
  DragMove(-kResizeSpan1, 0);
  // Alignment stays "RIGHT" during the drag because the only docked window
  // is being resized.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  // Release the mouse and the window should be attached to the edge.
  DragEnd();
  // The window should get docked.
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  // w1 is now wider and the dock should expand to be as wide as w1.
  EXPECT_EQ(previous_width + kResizeSpan1, w1->bounds().width());
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));

  // Undock by dragging almost to the left edge.
  DragToVerticalPositionRelativeToEdge(DOCKED_EDGE_LEFT, w1.get(), 100, 20);
  // Width should be preserved.
  EXPECT_EQ(previous_width + kResizeSpan1, w1->bounds().width());
  // Height should be restored to what it was originally.
  EXPECT_EQ(201, w1->bounds().height());

  // Dock again.
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // Width should be reset to initial ideal width (25px).
  EXPECT_EQ(ideal_width(), w1->bounds().width());

  // Undock again by dragging left.
  DragToVerticalPositionRelativeToEdge(DOCKED_EDGE_LEFT, w1.get(), 100, 20);
  // Width should be reset to what it was last time the window was not docked.
  EXPECT_EQ(previous_width + kResizeSpan1, w1->bounds().width());
  // Height should be restored to what it was originally.
  EXPECT_EQ(201, w1->bounds().height());
}

// Dock a window, resize it and test that it stays docked.
TEST_P(DockedWindowResizerTest, ResizingKeepsDockedState) {
  if (!SupportsHostWindowResize())
    return;

  // Wider display to start since panels are limited to half the display width.
  UpdateDisplay("1000x600");
  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // Window should be docked at the right edge.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));

  // Resize the window left by a bit and test that the dock expands.
  int previous_width = w1->bounds().width();
  const int kResizeSpan1 = 30;
  ASSERT_NO_FATAL_FAILURE(ResizeStartAtOffsetFromWindowOrigin(
      w1.get(), 0, 20, HTLEFT));
  DragMove(-kResizeSpan1, 0);
  // Normally alignment would be reset to "NONE" during the drag when there is
  // only a single window docked and it is being dragged. However because that
  // window is being resized rather than moved the alignment is not changed.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  // Release the mouse and the window should be attached to the edge.
  DragEnd();
  // The window should stay docked.
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  // w1 is now wider and the dock should expand to be as wide as w1.
  EXPECT_EQ(previous_width + kResizeSpan1, w1->bounds().width());
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));

  // Resize the window by dragging its right edge left a bit and test that the
  // window stays docked.
  previous_width = w1->bounds().width();
  const int kResizeSpan2 = 15;
  ASSERT_NO_FATAL_FAILURE(ResizeStartAtOffsetFromWindowOrigin(
      w1.get(), w1->bounds().width(), 20, HTRIGHT));
  DragMove(-kResizeSpan2, 0);
  // Alignment stays "RIGHT" during the drag because the window is being
  // resized rather than dragged.
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  // Release the mouse and the window should be attached to the edge.
  DragEnd();
  // The window should stay docked.
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  // The dock should stay as wide as w1 is now (a bit less than before).
  EXPECT_EQ(previous_width - kResizeSpan2, w1->bounds().width());
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));
}

// Dock two windows, resize one. Test the docked windows area size.
TEST_P(DockedWindowResizerTest, ResizeTwoWindows) {
  if (!SupportsHostWindowResize())
    return;

  // Wider display to start since panels are limited to half the display width.
  UpdateDisplay("1000x600");
  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 201)));

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 100);
  // Both windows should now be docked at the right edge.
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(kShellWindowId_DockedContainer, w2->parent()->id());
  // Dock width should be set to ideal width.
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(ideal_width(), docked_width(manager));

  // Resize the first window left by a bit and test that the dock expands.
  int previous_width = w1->bounds().width();
  const int kResizeSpan1 = 30;
  ASSERT_NO_FATAL_FAILURE(ResizeStartAtOffsetFromWindowOrigin(w1.get(),
                                                              0, 20,
                                                              HTLEFT));
  DragMove(-kResizeSpan1, 0);
  DragEnd();
  // w1 is now wider than before.
  EXPECT_EQ(previous_width + kResizeSpan1, w1->bounds().width());
  // Both windows should get resized since they both don't have min/max size.
  EXPECT_EQ(w1->bounds().width(), w2->bounds().width());
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));

  // Resize the second window left by a bit more and test that the dock expands.
  previous_width = w2->bounds().width();
  ASSERT_NO_FATAL_FAILURE(ResizeStartAtOffsetFromWindowOrigin(w2.get(),
                                                              0, 20,
                                                              HTLEFT));
  DragMove(-kResizeSpan1, 0);
  DragEnd();
  // w2 should get wider since it was resized by a user.
  EXPECT_EQ(previous_width + kResizeSpan1, w2->bounds().width());
  // w1 should stay as wide as w2 since both were flush with the dock edge.
  EXPECT_EQ(w2->bounds().width(), w1->bounds().width());
  EXPECT_EQ(w2->bounds().width(), docked_width(manager));

  // Undock w2 and then dock it back.
  DragToVerticalPositionRelativeToEdge(DOCKED_EDGE_RIGHT, w2.get(), -400, 100);
  EXPECT_EQ(kShellWindowId_DefaultContainer, w2->parent()->id());
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 100);
  EXPECT_EQ(kShellWindowId_DockedContainer, w2->parent()->id());
  // w2 should become same width as w1.
  EXPECT_EQ(w1->bounds().width(), w2->bounds().width());
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));

  // Make w1 even wider.
  ASSERT_NO_FATAL_FAILURE(ResizeStartAtOffsetFromWindowOrigin(w1.get(),
                                                              0, 20,
                                                              HTLEFT));
  DragMove(-kResizeSpan1, 0);
  DragEnd();
  // Making w1 wider should make both windows wider since w2 no longer remembers
  // user width.
  EXPECT_EQ(w1->bounds().width(), w2->bounds().width());
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));
}

// Tests that dragging a window down to shelf attaches a panel but does not
// attach a regular window.
TEST_P(DockedWindowResizerTest, DragToShelf) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  // Work area should cover the whole screen.
  EXPECT_EQ(ScreenUtil::GetDisplayBoundsInParent(w1.get()).width(),
            ScreenUtil::GetDisplayWorkAreaBoundsInParent(w1.get()).width());

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // A window should be docked at the right edge.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));

  // Detach and drag down to shelf.
  ASSERT_NO_FATAL_FAILURE(DragStart(w1.get()));
  DragMove(-40, 0);
  // Alignment is set to "NONE" when drag starts.
  EXPECT_EQ(DOCKED_ALIGNMENT_NONE, docked_alignment(manager));
  // Release the mouse and the window should be no longer attached to the edge.
  DragEnd();
  EXPECT_EQ(DOCKED_ALIGNMENT_NONE, docked_alignment(manager));

  // Drag down almost to shelf. A panel will snap, a regular window won't.
  ShelfWidget* shelf = Shelf::ForPrimaryDisplay()->shelf_widget();
  const int shelf_y = shelf->GetWindowBoundsInScreen().y();
  const int kDistanceFromShelf = 10;
  ASSERT_NO_FATAL_FAILURE(DragStart(w1.get()));
  DragMove(0, -kDistanceFromShelf + shelf_y - w1->bounds().bottom());
  DragEnd();
  if (test_panels()) {
    // The panel should be touching the shelf and attached.
    EXPECT_EQ(shelf_y, w1->bounds().bottom());
    EXPECT_TRUE(wm::GetWindowState(w1.get())->panel_attached());
  } else {
    // The window should not be touching the shelf.
    EXPECT_EQ(shelf_y - kDistanceFromShelf, w1->bounds().bottom());
  }
}

// Tests that docking and undocking a |window| with a transient child properly
// maintains the parent of that transient child to be the same as the |window|.
TEST_P(DockedWindowResizerTest, DragWindowWithTransientChild) {
  if (!SupportsHostWindowResize())
    return;

  // Create a window with a transient child.
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> child(CreateTestWindowInShellWithDelegateAndType(
      NULL, ui::wm::WINDOW_TYPE_NORMAL, 0, gfx::Rect(20, 20, 150, 20)));
  ::wm::AddTransientChild(window.get(), child.get());
  if (window->parent() != child->parent())
    window->parent()->AddChild(child.get());
  EXPECT_EQ(window.get(), ::wm::GetTransientParent(child.get()));

  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, window.get(), 20);

  // A window should be docked at the right edge.
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());
  EXPECT_EQ(kShellWindowId_DockedContainer, child->parent()->id());

  // Drag the child - it should move freely and stay where it is dragged.
  ASSERT_NO_FATAL_FAILURE(DragStart(child.get()));
  DragMove(500, 20);
  DragEnd();
  EXPECT_EQ(gfx::Point(20 + 500, 20 + 20).ToString(),
            child->GetBoundsInScreen().origin().ToString());

  // Undock the window by dragging left.
  ASSERT_NO_FATAL_FAILURE(DragStart(window.get()));
  DragMove(-32, -10);
  DragEnd();

  // The window should be undocked and the transient child should be reparented.
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
  EXPECT_EQ(kShellWindowId_DefaultContainer, child->parent()->id());
  // The child should not have moved.
  EXPECT_EQ(gfx::Point(20 + 500, 20 + 20).ToString(),
            child->GetBoundsInScreen().origin().ToString());
}

// Tests that reparenting windows during the drag does not affect system modal
// windows that are transient children of the dragged windows.
TEST_P(DockedWindowResizerTest, DragWindowWithModalTransientChild) {
  if (!SupportsHostWindowResize())
    return;

  // Create a window.
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  gfx::Rect bounds(window->bounds());

  // Start dragging the window.
  ASSERT_NO_FATAL_FAILURE(DragStart(window.get()));
  gfx::Vector2d move_vector(40, test_panels() ? -60 : 60);
  DragMove(move_vector.x(), move_vector.y());
  EXPECT_EQ(CorrectContainerIdDuringDrag(), window->parent()->id());

  // While still dragging create a modal window and make it a transient child of
  // the |window|.
  scoped_ptr<aura::Window> child(CreateModalWindow(gfx::Rect(20, 20, 150, 20)));
  ::wm::AddTransientChild(window.get(), child.get());
  EXPECT_EQ(window.get(), ::wm::GetTransientParent(child.get()));
  EXPECT_EQ(kShellWindowId_SystemModalContainer, child->parent()->id());

  // End the drag, the |window| should have moved (if it is a panel it will
  // no longer be attached to the shelf since we dragged it above).
  DragEnd();
  bounds.Offset(move_vector);
  EXPECT_EQ(bounds.ToString(), window->GetBoundsInScreen().ToString());

  // The original |window| should be in the default container (not docked or
  // attached).
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
  // The transient |child| should still be in system modal container.
  EXPECT_EQ(kShellWindowId_SystemModalContainer, child->parent()->id());
  // The |child| should not have moved.
  EXPECT_EQ(gfx::Point(20, 20).ToString(),
            child->GetBoundsInScreen().origin().ToString());
  // The |child| should still be a transient child of |window|.
  EXPECT_EQ(window.get(), ::wm::GetTransientParent(child.get()));
}

// Tests that side snapping a window undocks it, closes the dock and then snaps.
TEST_P(DockedWindowResizerTest, SideSnapDocked) {
  if (!SupportsHostWindowResize() || test_panels())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  wm::WindowState* window_state = wm::GetWindowState(w1.get());
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // A window should be docked at the right edge.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(w1->parent()->layout_manager());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));
  EXPECT_TRUE(window_state->IsDocked());
  EXPECT_FALSE(window_state->IsSnapped());

  // Side snap at right edge.
  const wm::WMEvent snap_right(wm::WM_EVENT_SNAP_RIGHT);
  window_state->OnWMEvent(&snap_right);
  // The window should be snapped at the right edge and the dock should close.
  gfx::Rect work_area(ScreenUtil::GetDisplayWorkAreaBoundsInParent(w1.get()));
  EXPECT_EQ(0, docked_width(manager));
  EXPECT_EQ(work_area.height(), w1->bounds().height());
  EXPECT_EQ(work_area.right(), w1->bounds().right());
  EXPECT_EQ(kShellWindowId_DefaultContainer, w1->parent()->id());
  EXPECT_FALSE(window_state->IsDocked());
  EXPECT_TRUE(window_state->IsSnapped());

  // Dock again.
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  // A window should be docked at the right edge.
  EXPECT_EQ(w1->GetRootWindow()->GetBoundsInScreen().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(w1->bounds().width(), docked_width(manager));
  EXPECT_TRUE(window_state->IsDocked());
  EXPECT_FALSE(window_state->IsSnapped());

  // Side snap at left edge.
  const wm::WMEvent snap_left(wm::WM_EVENT_SNAP_LEFT);
  window_state->OnWMEvent(&snap_left);
  // The window should be snapped at the right edge and the dock should close.
  EXPECT_EQ(work_area.ToString(),
            ScreenUtil::GetDisplayWorkAreaBoundsInParent(w1.get()).ToString());
  EXPECT_EQ(0, docked_width(manager));
  EXPECT_EQ(work_area.height(), w1->bounds().height());
  EXPECT_EQ(work_area.x(), w1->bounds().x());
  EXPECT_EQ(kShellWindowId_DefaultContainer, w1->parent()->id());
  EXPECT_FALSE(window_state->IsDocked());
  EXPECT_TRUE(window_state->IsSnapped());
}

// Tests that a window is undocked if the window is maximized via a keyboard
// accelerator during a drag.
TEST_P(DockedWindowResizerTest, MaximizedDuringDrag) {
  if (!SupportsHostWindowResize() || test_panels())
    return;

  scoped_ptr<aura::Window> window(CreateTestWindow(
      gfx::Rect(0, 0, ideal_width(), 201)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());

  // Dock the window to the right edge.
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, window.get(), 20);
  EXPECT_EQ(window->GetRootWindow()->GetBoundsInScreen().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(kShellWindowId_DockedContainer, window->parent()->id());
  DockedWindowLayoutManager* manager =
      static_cast<DockedWindowLayoutManager*>(
          window->parent()->layout_manager());
  EXPECT_EQ(DOCKED_ALIGNMENT_RIGHT, docked_alignment(manager));
  EXPECT_EQ(window->bounds().width(), docked_width(manager));
  EXPECT_TRUE(window_state->IsDocked());

  // Maximize the window while in a real drag. In particular,
  // ToplevelWindowEventHandler::ScopedWindowResizer::OnWindowStateTypeChanged()
  // must be called in order for the maximized window's size to be correct.
  delegate()->set_window_component(HTCAPTION);
  aura::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(window->GetBoundsInScreen().origin());
  generator.PressLeftButton();
  generator.MoveMouseBy(10, 10);
  window_state->Maximize();
  generator.ReleaseLeftButton();

  // |window| should get undocked.
  EXPECT_EQ(kShellWindowId_DefaultContainer, window->parent()->id());
  EXPECT_EQ(0, docked_width(manager));
  EXPECT_EQ(
      ScreenUtil::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window->bounds().ToString());
  EXPECT_TRUE(window_state->IsMaximized());
}

// Tests run twice - on both panels and normal windows
INSTANTIATE_TEST_CASE_P(NormalOrPanel,
                        DockedWindowResizerTest,
                        testing::Values(ui::wm::WINDOW_TYPE_NORMAL,
                                        ui::wm::WINDOW_TYPE_PANEL));

}  // namespace ash
