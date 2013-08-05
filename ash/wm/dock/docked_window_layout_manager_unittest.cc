// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dock/docked_window_layout_manager.h"

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
#include "ash/test/launcher_view_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_launcher_delegate.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_resizer.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

class DockedWindowLayoutManagerTest
    : public test::AshTestBase,
      public testing::WithParamInterface<aura::client::WindowType> {
 public:
  DockedWindowLayoutManagerTest() : window_type_(GetParam()) {}
  virtual ~DockedWindowLayoutManagerTest() {}

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableStickyEdges);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableDockedWindows);
    AshTestBase::SetUp();
    UpdateDisplay("600x600");
    ASSERT_TRUE(test::TestLauncherDelegate::instance());

    launcher_view_test_.reset(new test::LauncherViewTestAPI(
        Launcher::ForPrimaryDisplay()->GetLauncherViewForTest()));
    launcher_view_test_->SetAnimationDuration(1);
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
        NULL,
        window_type_,
        0,
        bounds);
    if (window_type_ == aura::client::WINDOW_TYPE_PANEL) {
      test::TestLauncherDelegate* launcher_delegate =
          test::TestLauncherDelegate::instance();
      launcher_delegate->AddLauncherItem(window);
      PanelLayoutManager* manager =
          static_cast<PanelLayoutManager*>(GetPanelContainer(window)->
              layout_manager());
      manager->Relayout();
    }
    return window;
  }

  aura::Window* GetPanelContainer(aura::Window* panel) {
    return Shell::GetContainer(panel->GetRootWindow(),
                               internal::kShellWindowId_PanelContainer);
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

 private:
  scoped_ptr<WindowResizer> resizer_;
  scoped_ptr<test::LauncherViewTestAPI> launcher_view_test_;
  aura::client::WindowType window_type_;

  // Location at start of the drag in |window->parent()|'s coordinates.
  gfx::Point initial_location_in_parent_;

  DISALLOW_COPY_AND_ASSIGN(DockedWindowLayoutManagerTest);
};

// Tests that a created window is successfully added to the dock
// layout manager.
TEST_P(DockedWindowLayoutManagerTest, AddOneWindow) {
  if (!SupportsHostWindowResize())
    return;

  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  DragRelativeToEdge(DOCKED_EDGE_RIGHT, window.get(), 0);

  // The window should be attached and snapped to the right side of the screen.
  EXPECT_EQ(window->GetRootWindow()->bounds().right(),
            window->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, window->parent()->id());
}

// Adds two windows and tests that the gaps are evenly distributed.
TEST_P(DockedWindowLayoutManagerTest, AddTwoWindows) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 202)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 300);

  // The windows should be attached and snapped to the right side of the screen.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(w2->GetRootWindow()->bounds().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());

  // Test that the gaps differ at most by a single pixel.
  int gap1 = w1->GetBoundsInScreen().y();
  int gap2 = w2->GetBoundsInScreen().y() - w1->GetBoundsInScreen().bottom();
  int gap3 = ScreenAsh::GetDisplayWorkAreaBoundsInParent(w1.get()).bottom() -
      w2->GetBoundsInScreen().bottom();
  EXPECT_LE(abs(gap1 - gap2), 1);
  EXPECT_LE(abs(gap2 - gap3), 1);
  EXPECT_LE(abs(gap3 - gap1), 1);
}

// Adds two non-overlapping windows and tests layout after a drag.
TEST_P(DockedWindowLayoutManagerTest, TwoWindowsDragging) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 202)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 300);

  // The windows should be attached and snapped to the right side of the screen.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(w2->GetRootWindow()->bounds().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());

  // Drag w2 above w1.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(w2.get(), 0, 20));
  DragMove(0, w1->bounds().y() - w2->bounds().y() - 20);
  DragEnd();

  // Test the new windows order and that the gaps differ at most by a pixel.
  int gap1 = w2->GetBoundsInScreen().y();
  int gap2 = w1->GetBoundsInScreen().y() - w2->GetBoundsInScreen().bottom();
  int gap3 = ScreenAsh::GetDisplayWorkAreaBoundsInParent(w1.get()).bottom() -
      w1->GetBoundsInScreen().bottom();
  EXPECT_LE(abs(gap1 - gap2), 1);
  EXPECT_LE(abs(gap2 - gap3), 1);
  EXPECT_LE(abs(gap3 - gap1), 1);
}

// Adds three overlapping windows and tests layout after a drag.
TEST_P(DockedWindowLayoutManagerTest, ThreeWindowsDragging) {
  if (!SupportsHostWindowResize())
    return;

  scoped_ptr<aura::Window> w1(CreateTestWindow(gfx::Rect(0, 0, 201, 201)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w1.get(), 20);
  scoped_ptr<aura::Window> w2(CreateTestWindow(gfx::Rect(0, 0, 210, 202)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w2.get(), 100);
  scoped_ptr<aura::Window> w3(CreateTestWindow(gfx::Rect(0, 0, 220, 204)));
  DragToVerticalPositionAndToEdge(DOCKED_EDGE_RIGHT, w3.get(), 300);

  // All windows should be attached and snapped to the right side of the screen.
  EXPECT_EQ(w1->GetRootWindow()->bounds().right(),
            w1->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w1->parent()->id());
  EXPECT_EQ(w2->GetRootWindow()->bounds().right(),
            w2->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w2->parent()->id());
  EXPECT_EQ(w3->GetRootWindow()->bounds().right(),
            w3->GetBoundsInScreen().right());
  EXPECT_EQ(internal::kShellWindowId_DockedContainer, w3->parent()->id());

  // Test that the top and bottom windows are clamped in work area and
  // that the overlaps between the windows differ at most by a pixel.
  int overlap1 = w1->GetBoundsInScreen().y();
  int overlap2 = w1->GetBoundsInScreen().bottom() - w2->GetBoundsInScreen().y();
  int overlap3 = w2->GetBoundsInScreen().bottom() - w3->GetBoundsInScreen().y();
  int overlap4 =
      ScreenAsh::GetDisplayWorkAreaBoundsInParent(w3.get()).bottom() -
      w3->GetBoundsInScreen().bottom();
  EXPECT_EQ(0, overlap1);
  EXPECT_LE(abs(overlap2 - overlap3), 1);
  EXPECT_EQ(0, overlap4);

  // Drag w1 below w2.
  ASSERT_NO_FATAL_FAILURE(DragStartAtOffsetFromwindowOrigin(w1.get(), 0, 20));
  DragMove(0, w2->bounds().y() - w1->bounds().y() + 20);

  // During the drag the windows get rearranged and the top and the bottom
  // should be clamped by the work area.
  EXPECT_EQ(0, w2->GetBoundsInScreen().y());
  EXPECT_GT(w1->GetBoundsInScreen().y(), w2->GetBoundsInScreen().y());
  EXPECT_EQ(ScreenAsh::GetDisplayWorkAreaBoundsInParent(w3.get()).bottom(),
            w3->GetBoundsInScreen().bottom());
  DragEnd();

  // Test the new windows order and that the overlaps differ at most by a pixel.
  overlap1 = w2->GetBoundsInScreen().y();
  overlap2 = w2->GetBoundsInScreen().bottom() - w1->GetBoundsInScreen().y();
  overlap3 = w1->GetBoundsInScreen().bottom() - w3->GetBoundsInScreen().y();
  overlap4 = ScreenAsh::GetDisplayWorkAreaBoundsInParent(w3.get()).bottom() -
      w3->GetBoundsInScreen().bottom();
  EXPECT_EQ(0, overlap1);
  EXPECT_LE(abs(overlap2 - overlap3), 1);
  EXPECT_EQ(0, overlap4);
}

// Tests run twice - on both panels and normal windows
INSTANTIATE_TEST_CASE_P(NormalOrPanel,
                        DockedWindowLayoutManagerTest,
                        testing::Values(aura::client::WINDOW_TYPE_NORMAL,
                                        aura::client::WINDOW_TYPE_PANEL));
}  // namespace internal
}  // namespace ash
