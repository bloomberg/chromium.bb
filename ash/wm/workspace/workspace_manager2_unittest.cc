// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_manager2.h"

#include "ash/ash_switches.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/property_util.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace2.h"
#include "ash/wm/workspace_controller_test_helper.h"
#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

using aura::Window;

namespace ash {
namespace internal {

namespace {

bool GetWindowOverlapsShelf() {
  return Shell::GetInstance()->shelf()->window_overlaps_shelf();
}

}  // namespace

class WorkspaceManager2Test : public test::AshTestBase {
 public:
  WorkspaceManager2Test() : manager_(NULL) {}
  virtual ~WorkspaceManager2Test() {}

  aura::Window* CreateTestWindowUnparented() {
    aura::Window* window = new aura::Window(NULL);
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    return window;
  }

  aura::Window* CreateTestWindow() {
    aura::Window* window = new aura::Window(NULL);
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    window->SetParent(NULL);
    return window;
  }

  aura::Window* GetViewport() {
    return Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                               kShellWindowId_DefaultContainer);
  }

  const std::vector<Workspace2*>& workspaces() const {
    return manager_->workspaces_;
  }

  gfx::Rect GetFullscreenBounds(aura::Window* window) {
    return gfx::Screen::GetDisplayNearestWindow(window).bounds();
  }

  Workspace2* active_workspace() {
    return manager_->active_workspace_;
  }

  Workspace2* FindBy(aura::Window* window) const {
    return manager_->FindBy(window);
  }

  std::string WorkspaceStateString(Workspace2* workspace) {
    return (workspace->is_maximized() ? "M" : "") +
        base::IntToString(static_cast<int>(
                              workspace->window()->children().size()));
  }

  int active_index() {
    return static_cast<int>(
        manager_->FindWorkspace(manager_->active_workspace_) -
        manager_->workspaces_.begin());
  }

  std::string StateString() {
    std::string result;
    for (size_t i = 0; i < manager_->workspaces_.size(); ++i) {
      if (i > 0)
        result += " ";
      result += WorkspaceStateString(manager_->workspaces_[i]);
    }

    if (!manager_->pending_workspaces_.empty()) {
      result += " P=";
      for (std::set<Workspace2*>::const_iterator i =
               manager_->pending_workspaces_.begin();
           i != manager_->pending_workspaces_.end(); ++i) {
        if (i != manager_->pending_workspaces_.begin())
          result += " ";
        result += WorkspaceStateString(*i);
      }
    }

    result += " active=" + base::IntToString(active_index());
    return result;
  }

  // Overridden from AshTestBase:
  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    WorkspaceControllerTestHelper workspace_helper(
        Shell::TestApi(Shell::GetInstance()).workspace_controller());
    manager_ = workspace_helper.workspace_manager2();
  }

  virtual void TearDown() OVERRIDE {
    manager_ = NULL;
    test::AshTestBase::TearDown();
  }

 protected:
  WorkspaceManager2* manager_;

 private:
  scoped_ptr<ActivationController> activation_controller_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceManager2Test);
};

// Assertions around adding a normal window.
TEST_F(WorkspaceManager2Test, AddNormalWindowWhenEmpty) {
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->SetBounds(gfx::Rect(0, 0, 250, 251));

  EXPECT_TRUE(GetRestoreBoundsInScreen(w1.get()) == NULL);

  w1->Show();

  EXPECT_TRUE(GetRestoreBoundsInScreen(w1.get()) == NULL);

  ASSERT_TRUE(w1->layer() != NULL);
  EXPECT_TRUE(w1->layer()->visible());

  EXPECT_EQ("0,0 250x251", w1->bounds().ToString());

  // Should be 1 workspace for the desktop, not maximized.
  ASSERT_EQ("1 active=0", StateString());
  EXPECT_EQ(w1.get(), workspaces()[0]->window()->children()[0]);
}

// Assertions around maximizing/unmaximizing.
TEST_F(WorkspaceManager2Test, SingleMaximizeWindow) {
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->SetBounds(gfx::Rect(0, 0, 250, 251));

  w1->Show();
  wm::ActivateWindow(w1.get());

  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  ASSERT_TRUE(w1->layer() != NULL);
  EXPECT_TRUE(w1->layer()->visible());

  EXPECT_EQ("0,0 250x251", w1->bounds().ToString());

  // Maximize the window.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);

  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Should be 2 workspaces, the second maximized with w1.
  ASSERT_EQ("0 M1 active=1", StateString());
  EXPECT_EQ(w1.get(), workspaces()[1]->window()->children()[0]);
  EXPECT_EQ(ScreenAsh::GetMaximizedWindowBoundsInParent(w1.get()).width(),
            w1->bounds().width());
  EXPECT_EQ(ScreenAsh::GetMaximizedWindowBoundsInParent(w1.get()).height(),
            w1->bounds().height());

  // Restore the window.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);

  // Should be 1 workspace for the desktop.
  ASSERT_EQ("1 active=0", StateString());
  EXPECT_EQ(w1.get(), workspaces()[0]->window()->children()[0]);
  EXPECT_EQ("0,0 250x251", w1->bounds().ToString());
}

// Assertions around closing the last window in a workspace.
TEST_F(WorkspaceManager2Test, CloseLastWindowInWorkspace) {
  scoped_ptr<Window> w1(CreateTestWindow());
  scoped_ptr<Window> w2(CreateTestWindow());
  w1->SetBounds(gfx::Rect(0, 0, 250, 251));
  w1->Show();
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w2->Show();
  wm::ActivateWindow(w1.get());

  // Should be 1 workspace and 1 pending, !maximized and maximized. The second
  // workspace is pending since the window wasn't active.
  ASSERT_EQ("1 P=M1 active=0", StateString());
  EXPECT_EQ(w1.get(), workspaces()[0]->window()->children()[0]);

  // Close w2.
  w2.reset();

  // Should have one workspace.
  ASSERT_EQ("1 active=0", StateString());
  EXPECT_EQ(w1.get(), workspaces()[0]->window()->children()[0]);
  EXPECT_TRUE(w1->IsVisible());
}

// Assertions around adding a maximized window when empty.
TEST_F(WorkspaceManager2Test, AddMaximizedWindowWhenEmpty) {
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->SetBounds(gfx::Rect(0, 0, 250, 251));
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w1->Show();
  wm::ActivateWindow(w1.get());

  ASSERT_TRUE(w1->layer() != NULL);
  EXPECT_TRUE(w1->layer()->visible());
  gfx::Rect work_area(
      ScreenAsh::GetMaximizedWindowBoundsInParent(w1.get()));
  EXPECT_EQ(work_area.width(), w1->bounds().width());
  EXPECT_EQ(work_area.height(), w1->bounds().height());

  // Should be 2 workspaces (since we always keep the desktop).
  ASSERT_EQ("0 M1 active=1", StateString());
  EXPECT_EQ(w1.get(), workspaces()[1]->window()->children()[0]);
}

// Assertions around two windows and toggling one to be maximized.
TEST_F(WorkspaceManager2Test, MaximizeWithNormalWindow) {
  scoped_ptr<Window> w1(CreateTestWindow());
  scoped_ptr<Window> w2(CreateTestWindow());
  w1->SetBounds(gfx::Rect(0, 0, 250, 251));
  w1->Show();

  ASSERT_TRUE(w1->layer() != NULL);
  EXPECT_TRUE(w1->layer()->visible());

  w2->SetBounds(gfx::Rect(0, 0, 50, 51));
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w2->Show();
  wm::ActivateWindow(w2.get());

  // Should now be two workspaces.
  ASSERT_EQ("1 M1 active=1", StateString());
  EXPECT_EQ(w1.get(), workspaces()[0]->window()->children()[0]);
  EXPECT_EQ(w2.get(), workspaces()[1]->window()->children()[0]);

  gfx::Rect work_area(ScreenAsh::GetMaximizedWindowBoundsInParent(w1.get()));
  EXPECT_EQ(work_area.width(), w2->bounds().width());
  EXPECT_EQ(work_area.height(), w2->bounds().height());

  // Restore w2, which should then go back to one workspace.
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  ASSERT_EQ("2 active=0", StateString());
  EXPECT_EQ(w1.get(), workspaces()[0]->window()->children()[0]);
  EXPECT_EQ(w2.get(), workspaces()[0]->window()->children()[1]);
  EXPECT_EQ(50, w2->bounds().width());
  EXPECT_EQ(51, w2->bounds().height());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
}

// Assertions around two maximized windows.
TEST_F(WorkspaceManager2Test, TwoMaximized) {
  scoped_ptr<Window> w1(CreateTestWindow());
  scoped_ptr<Window> w2(CreateTestWindow());
  w1->SetBounds(gfx::Rect(0, 0, 250, 251));
  w1->Show();
  wm::ActivateWindow(w1.get());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  ASSERT_EQ("1 M1 active=1", StateString());

  w2->SetBounds(gfx::Rect(0, 0, 50, 51));
  w2->Show();
  wm::ActivateWindow(w2.get());
  ASSERT_EQ("1 M1 active=0", StateString());

  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  ASSERT_EQ("0 M1 M1 active=2", StateString());

  // The last stacked window (|w2|) should be last since it was maximized last.
  EXPECT_EQ(w1.get(), workspaces()[1]->window()->children()[0]);
  EXPECT_EQ(w2.get(), workspaces()[2]->window()->children()[0]);
}

// Makes sure requests to change the bounds of a normal window go through.
TEST_F(WorkspaceManager2Test, ChangeBoundsOfNormalWindow) {
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->Show();

  // Setting the bounds should go through since the window is in the normal
  // workspace.
  w1->SetBounds(gfx::Rect(0, 0, 200, 500));
  EXPECT_EQ(200, w1->bounds().width());
  EXPECT_EQ(500, w1->bounds().height());
}

// Verifies the bounds is not altered when showing and grid is enabled.
TEST_F(WorkspaceManager2Test, SnapToGrid) {
  scoped_ptr<Window> w1(CreateTestWindowUnparented());
  w1->SetBounds(gfx::Rect(1, 6, 25, 30));
  w1->SetParent(NULL);
  // We are not aligning this anymore this way. When the window gets shown
  // the window is expected to be handled differently, but this cannot be
  // tested with this test. So the result of this test should be that the
  // bounds are exactly as passed in.
  EXPECT_EQ("1,6 25x30", w1->bounds().ToString());
}

// Assertions around a fullscreen window.
TEST_F(WorkspaceManager2Test, SingleFullscreenWindow) {
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->SetBounds(gfx::Rect(0, 0, 250, 251));
  // Make the window fullscreen.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  w1->Show();
  wm::ActivateWindow(w1.get());

  // Should be 2 workspaces, normal and maximized.
  ASSERT_EQ("0 M1 active=1", StateString());
  EXPECT_EQ(w1.get(), workspaces()[1]->window()->children()[0]);
  EXPECT_EQ(GetFullscreenBounds(w1.get()).width(), w1->bounds().width());
  EXPECT_EQ(GetFullscreenBounds(w1.get()).height(), w1->bounds().height());

  // Restore the window. Use SHOW_STATE_DEFAULT as that is what we'll end up
  // with when using views::Widget.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_DEFAULT);
  EXPECT_EQ("0,0 250x251", w1->bounds().ToString());

  // Should be 1 workspace for the desktop.
  ASSERT_EQ("1 active=0", StateString());
  EXPECT_EQ(w1.get(), workspaces()[0]->window()->children()[0]);
  EXPECT_EQ(250, w1->bounds().width());
  EXPECT_EQ(251, w1->bounds().height());

  // Back to fullscreen.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  ASSERT_EQ("0 M1 active=1", StateString());
  EXPECT_EQ(w1.get(), workspaces()[1]->window()->children()[0]);
  EXPECT_EQ(GetFullscreenBounds(w1.get()).width(), w1->bounds().width());
  EXPECT_EQ(GetFullscreenBounds(w1.get()).height(), w1->bounds().height());
  ASSERT_TRUE(GetRestoreBoundsInScreen(w1.get()));
  EXPECT_EQ("0,0 250x251", GetRestoreBoundsInScreen(w1.get())->ToString());
}

// Makes sure switching workspaces doesn't show transient windows.
TEST_F(WorkspaceManager2Test, DontShowTransientsOnSwitch) {
  scoped_ptr<Window> w1(CreateTestWindow());
  scoped_ptr<Window> w2(CreateTestWindow());

  w1->SetBounds(gfx::Rect(0, 0, 250, 251));
  w2->SetBounds(gfx::Rect(0, 0, 250, 251));
  w1->AddTransientChild(w2.get());

  w1->Show();

  scoped_ptr<Window> w3(CreateTestWindow());
  w3->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w3->Show();
  wm::ActivateWindow(w3.get());

  EXPECT_FALSE(w1->layer()->IsDrawn());
  EXPECT_FALSE(w2->layer()->IsDrawn());
  EXPECT_TRUE(w3->layer()->IsDrawn());

  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(w1->layer()->IsDrawn());
  EXPECT_FALSE(w2->layer()->IsDrawn());
  EXPECT_FALSE(w3->layer()->IsDrawn());
}

// Assertions around minimizing a single window.
TEST_F(WorkspaceManager2Test, MinimizeSingleWindow) {
  scoped_ptr<Window> w1(CreateTestWindow());

  w1->Show();
  ASSERT_EQ("1 active=0", StateString());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  ASSERT_EQ("1 active=0", StateString());
  EXPECT_FALSE(w1->layer()->IsDrawn());

  // Show the window.
  w1->Show();
  EXPECT_TRUE(wm::IsWindowNormal(w1.get()));
  ASSERT_EQ("1 active=0", StateString());
  EXPECT_TRUE(w1->layer()->IsDrawn());
}

// Assertions around minimizing a maximized window.
TEST_F(WorkspaceManager2Test, MinimizeMaximizedWindow) {
  // Two windows, w1 normal, w2 maximized.
  scoped_ptr<Window> w1(CreateTestWindow());
  scoped_ptr<Window> w2(CreateTestWindow());
  w1->Show();
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w2->Show();
  wm::ActivateWindow(w2.get());
  ASSERT_EQ("1 M1 active=1", StateString());

  // Minimize w2.
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  ASSERT_EQ("1 P=M1 active=0", StateString());
  EXPECT_TRUE(w1->layer()->IsDrawn());
  EXPECT_FALSE(w2->layer()->IsDrawn());

  // Show the window, which should trigger unminimizing.
  w2->Show();
  ASSERT_EQ("1 P=M1 active=0", StateString());

  wm::ActivateWindow(w2.get());
  ASSERT_EQ("1 M1 active=1", StateString());

  EXPECT_TRUE(wm::IsWindowMaximized(w2.get()));
  EXPECT_FALSE(w1->layer()->IsDrawn());
  EXPECT_TRUE(w2->layer()->IsDrawn());

  // Minimize the window, which should hide the window and activate another.
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_FALSE(wm::IsActiveWindow(w2.get()));
  EXPECT_FALSE(w2->layer()->IsDrawn());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Make the window normal.
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  ASSERT_EQ("2 active=0", StateString());
  EXPECT_EQ(w1.get(), workspaces()[0]->window()->children()[0]);
  EXPECT_EQ(w2.get(), workspaces()[0]->window()->children()[1]);
  EXPECT_TRUE(w2->layer()->IsDrawn());
}

// Verifies ShelfLayoutManager's visibility/auto-hide state is correctly
// updated.
TEST_F(WorkspaceManager2Test, ShelfStateUpdated) {
  // Since ShelfLayoutManager queries for mouse location, move the mouse so
  // it isn't over the shelf.
  aura::test::EventGenerator generator(
      Shell::GetPrimaryRootWindow(), gfx::Point());
  generator.MoveMouseTo(0, 0);

  scoped_ptr<Window> w1(CreateTestWindow());
  const gfx::Rect w1_bounds(0, 1, 101, 102);
  ShelfLayoutManager* shelf = Shell::GetInstance()->shelf();
  const gfx::Rect touches_shelf_bounds(
      0, shelf->GetIdealBounds().y() - 10, 101, 102);
  // Move |w1| to overlap the shelf.
  w1->SetBounds(touches_shelf_bounds);
  EXPECT_FALSE(GetWindowOverlapsShelf());

  // A visible ignored window should not trigger the overlap.
  scoped_ptr<Window> w_ignored(CreateTestWindow());
  w_ignored->SetBounds(touches_shelf_bounds);
  SetIgnoredByShelf(&(*w_ignored), true);
  w_ignored->Show();
  EXPECT_FALSE(GetWindowOverlapsShelf());

  // Make it visible, since visible shelf overlaps should be true.
  w1->Show();
  EXPECT_TRUE(GetWindowOverlapsShelf());

  wm::ActivateWindow(w1.get());
  w1->SetBounds(w1_bounds);
  w1->Show();
  wm::ActivateWindow(w1.get());

  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());

  // Maximize the window.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Restore.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());

  // Fullscreen.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(ShelfLayoutManager::HIDDEN, shelf->visibility_state());

  // Normal.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());
  EXPECT_FALSE(GetWindowOverlapsShelf());

  // Move window so it obscures shelf.
  w1->SetBounds(touches_shelf_bounds);
  EXPECT_TRUE(GetWindowOverlapsShelf());

  // Move it back.
  w1->SetBounds(w1_bounds);
  EXPECT_FALSE(GetWindowOverlapsShelf());

  // Maximize again.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Minimize.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());

  // Since the restore from minimize will restore to the pre-minimize
  // state (tested elsewhere), we abandon the current size and restore
  // rect and set them to the window.
  gfx::Rect restore = *GetRestoreBoundsInScreen(w1.get());
  EXPECT_EQ("0,0 800x598", w1->bounds().ToString());
  EXPECT_EQ("0,1 101x102", restore.ToString());
  ClearRestoreBounds(w1.get());
  w1->SetBounds(restore);

  // Restore.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());

  // Create another window, maximized.
  scoped_ptr<Window> w2(CreateTestWindow());
  w2->SetBounds(gfx::Rect(10, 11, 250, 251));
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w2->Show();
  wm::ActivateWindow(w2.get());
  EXPECT_EQ(1, active_index());
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());

  // Switch to w1.
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(0, active_index());
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());
  EXPECT_EQ(ScreenAsh::GetMaximizedWindowBoundsInParent(
                w2->parent()).ToString(),
            w2->bounds().ToString());

  // Switch to w2.
  wm::ActivateWindow(w2.get());
  EXPECT_EQ(1, active_index());
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(ShelfLayoutManager::AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());
  EXPECT_EQ(ScreenAsh::GetMaximizedWindowBoundsInParent(w2.get()).ToString(),
            w2->bounds().ToString());

  // Turn off auto-hide, switch back to w2 (maximized) and verify overlap.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  wm::ActivateWindow(w2.get());
  EXPECT_FALSE(GetWindowOverlapsShelf());

  // Move w1 to overlap shelf, it shouldn't change window overlaps shelf since
  // the window isn't in the visible workspace.
  w1->SetBounds(touches_shelf_bounds);
  EXPECT_FALSE(GetWindowOverlapsShelf());

  // Activate w1. Since w1 is visible the overlap state should be true.
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(GetWindowOverlapsShelf());
}

// Verifies persist across all workspaces.
TEST_F(WorkspaceManager2Test, PersistAcrossAllWorkspaces) {
  // Create a maximized window.
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->Show();
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  wm::ActivateWindow(w1.get());
  ASSERT_EQ("0 M1 active=1", StateString());

  // Create a window that persists across all workspaces. It should be placed in
  // the current maximized workspace.
  scoped_ptr<Window> w2(CreateTestWindow());
  SetPersistsAcrossAllWorkspaces(
      w2.get(),
      WINDOW_PERSISTS_ACROSS_ALL_WORKSPACES_VALUE_YES);
  w2->Show();
  ASSERT_EQ("1 M1 active=1", StateString());

  // Activate w2, which should move it to the 2nd workspace.
  wm::ActivateWindow(w2.get());
  ASSERT_EQ("0 M2 active=1", StateString());

  // Restoring w2 should drop the persists window back to the desktop, and drop
  // it to the bottom of the stack.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  ASSERT_EQ("2 active=0", StateString());
  EXPECT_EQ(w2.get(), workspaces()[0]->window()->children()[0]);
  EXPECT_EQ(w1.get(), workspaces()[0]->window()->children()[1]);

  // Repeat, but this time minimize. The minimized window should end up in
  // pending.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  ASSERT_EQ("1 P=M1 active=0", StateString());
  w2.reset(CreateTestWindow());
  SetPersistsAcrossAllWorkspaces(
      w2.get(),
      WINDOW_PERSISTS_ACROSS_ALL_WORKSPACES_VALUE_YES);
  w2->Show();
  ASSERT_EQ("1 P=M1 active=0", StateString());
  wm::ActivateWindow(w2.get());
  ASSERT_EQ("1 P=M1 active=0", StateString());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  ASSERT_EQ("1 P=M1 active=0", StateString());
  EXPECT_EQ(w2.get(), workspaces()[0]->window()->children()[0]);
}

// Verifies that when a window persists across all workpaces is activated that
// it moves to the current workspace.
TEST_F(WorkspaceManager2Test, ActivatePersistAcrossAllWorkspacesWhenNotActive) {
  // Create a window that persists across all workspaces.
  scoped_ptr<Window> w2(CreateTestWindow());
  SetPersistsAcrossAllWorkspaces(
      w2.get(),
      WINDOW_PERSISTS_ACROSS_ALL_WORKSPACES_VALUE_YES);
  w2->Show();
  ASSERT_EQ("1 active=0", StateString());

  // Create a maximized window.
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->Show();
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  wm::ActivateWindow(w1.get());
  ASSERT_EQ("1 M1 active=1", StateString());

  // Activate the persists across all workspace window. It should move to the
  // current workspace.
  wm::ActivateWindow(w2.get());
  ASSERT_EQ("0 M2 active=1", StateString());
  // The window that persists across all workspaces should be moved to the top
  // of the stacking order.
  EXPECT_EQ(w1.get(), workspaces()[1]->window()->children()[0]);
  EXPECT_EQ(w2.get(), workspaces()[1]->window()->children()[1]);
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
}

// Verifies Show()ing a minimized window that persists across all workspaces
// unminimizes the window.
TEST_F(WorkspaceManager2Test, ShowMinimizedPersistWindow) {
  // Create a window that persists across all workspaces.
  scoped_ptr<Window> w1(CreateTestWindow());
  SetPersistsAcrossAllWorkspaces(
      w1.get(),
      WINDOW_PERSISTS_ACROSS_ALL_WORKSPACES_VALUE_YES);
  w1->Show();
  wm::ActivateWindow(w1.get());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_FALSE(w1->IsVisible());
  w1->Show();
  EXPECT_TRUE(w1->IsVisible());
}

// Test that we report we're in the fullscreen state even if the fullscreen
// window isn't being managed by us (http://crbug.com/123931).
TEST_F(WorkspaceManager2Test, GetWindowStateWithUnmanagedFullscreenWindow) {
  ShelfLayoutManager* shelf = Shell::GetInstance()->shelf();

  // We need to create a regular window first so there's an active workspace.
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->Show();

  scoped_ptr<Window> w2(CreateTestWindow());
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  SetPersistsAcrossAllWorkspaces(
      w2.get(),
      WINDOW_PERSISTS_ACROSS_ALL_WORKSPACES_VALUE_YES);
  w2->Show();
  wm::ActivateWindow(w2.get());

  ASSERT_EQ("1 M1 active=1", StateString());

  EXPECT_EQ(ShelfLayoutManager::HIDDEN, shelf->visibility_state());
  EXPECT_EQ(WORKSPACE_WINDOW_STATE_FULL_SCREEN, manager_->GetWindowState());

  w2->Hide();
  ASSERT_EQ("1 P=M1 active=0", StateString());
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());

  w2->Show();
  ASSERT_EQ("1 P=M1 active=0", StateString());
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());
  EXPECT_EQ(WORKSPACE_WINDOW_STATE_DEFAULT, manager_->GetWindowState());

  wm::ActivateWindow(w2.get());
  ASSERT_EQ("1 M1 active=1", StateString());
  EXPECT_EQ(ShelfLayoutManager::HIDDEN, shelf->visibility_state());
  EXPECT_EQ(WORKSPACE_WINDOW_STATE_FULL_SCREEN, manager_->GetWindowState());

  w2.reset();
  ASSERT_EQ("1 active=0", StateString());
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());
  EXPECT_EQ(WORKSPACE_WINDOW_STATE_DEFAULT, manager_->GetWindowState());
}

// Variant of GetWindowStateWithUnmanagedFullscreenWindow that uses a maximized
// window rather than a normal window.
TEST_F(WorkspaceManager2Test,
       GetWindowStateWithUnmanagedFullscreenWindowWithMaximized) {
  ShelfLayoutManager* shelf = Shell::GetInstance()->shelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

  // Make the first window maximized.
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w1->Show();

  scoped_ptr<Window> w2(CreateTestWindow());
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  SetPersistsAcrossAllWorkspaces(
      w2.get(),
      WINDOW_PERSISTS_ACROSS_ALL_WORKSPACES_VALUE_YES);
  w2->Show();
  wm::ActivateWindow(w2.get());

  // Even though auto-hide behavior is NEVER full-screen windows cause the shelf
  // to hide.
  EXPECT_EQ(ShelfLayoutManager::HIDDEN, shelf->visibility_state());
  EXPECT_EQ(WORKSPACE_WINDOW_STATE_FULL_SCREEN,
            manager_->GetWindowState());

  w2->Hide();
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());

  w2->Show();
  wm::ActivateWindow(w2.get());
  EXPECT_EQ(ShelfLayoutManager::HIDDEN, shelf->visibility_state());
  EXPECT_EQ(WORKSPACE_WINDOW_STATE_FULL_SCREEN,
            manager_->GetWindowState());

  w2.reset();
  EXPECT_EQ(ShelfLayoutManager::VISIBLE, shelf->visibility_state());
}

// Verifies a window marked as persisting across all workspaces ends up in its
// own workspace when maximized.
TEST_F(WorkspaceManager2Test, MaximizeDontPersistEndsUpInOwnWorkspace) {
  scoped_ptr<Window> w1(CreateTestWindow());

  SetPersistsAcrossAllWorkspaces(
      w1.get(),
      WINDOW_PERSISTS_ACROSS_ALL_WORKSPACES_VALUE_YES);
  w1->Show();

  ASSERT_EQ("1 active=0", StateString());

  // Maximize should trigger containing the window.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  ASSERT_EQ("0 P=M1 active=0", StateString());

  // And resetting to normal should remove it.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  ASSERT_EQ("1 active=0", StateString());
}

// Verifies going from maximized to minimized sets the right state for painting
// the background of the launcher.
TEST_F(WorkspaceManager2Test, MinimizeResetsVisibility) {
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(ShelfLayoutManager::VISIBLE,
            Shell::GetInstance()->shelf()->visibility_state());
  EXPECT_FALSE(Shell::GetInstance()->launcher()->paints_background());
}

// Verifies transients are moved when maximizing.
TEST_F(WorkspaceManager2Test, MoveTransientOnMaximize) {
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->Show();
  scoped_ptr<Window> w2(CreateTestWindow());
  w1->AddTransientChild(w2.get());
  w2->Show();
  wm::ActivateWindow(w1.get());
  ASSERT_EQ("2 active=0", StateString());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  ASSERT_EQ("0 M2 active=1", StateString());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Create another transient child of |w1|. We do this unparented, set up the
  // transient parent then set parent. This is how NativeWidgetAura does things
  // too.
  scoped_ptr<Window> w3(CreateTestWindowUnparented());
  w1->AddTransientChild(w3.get());
  w3->SetParent(NULL);
  w3->Show();
  ASSERT_EQ("0 M3 active=1", StateString());

  // Minimize the window. All the transients are hidden as a result, so it ends
  // up in pending.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  ASSERT_EQ("0 P=M3 active=0", StateString());

  // Restore and everything should go back to the first workspace.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  ASSERT_EQ("3 active=0", StateString());
}

// Verifies window visibility during various workspace changes.
TEST_F(WorkspaceManager2Test, VisibilityTests) {
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->Show();
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_EQ(1.0f, w1->layer()->GetCombinedOpacity());

  // Create another window, activate it and maximized it.
  scoped_ptr<Window> w2(CreateTestWindow());
  w2->Show();
  wm::ActivateWindow(w2.get());
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_EQ(1.0f, w2->layer()->GetCombinedOpacity());
  EXPECT_FALSE(w1->IsVisible());

  // Switch to w1. |w1| should be visible and |w2| hidden.
  wm::ActivateWindow(w1.get());
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_EQ(1.0f, w1->layer()->GetCombinedOpacity());
  EXPECT_FALSE(w2->IsVisible());

  // Switch back to |w2|.
  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_EQ(1.0f, w2->layer()->GetCombinedOpacity());
  EXPECT_FALSE(w1->IsVisible());

  // Restore |w2|, both windows should be visible.
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_EQ(1.0f, w1->layer()->GetCombinedOpacity());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_EQ(1.0f, w2->layer()->GetCombinedOpacity());

  // Maximize |w2| again, then close it.
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w2->Hide();
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_EQ(1.0f, w1->layer()->GetCombinedOpacity());
  EXPECT_TRUE(w1->IsVisible());

  // Create |w2| and make it fullscreen.
  w2.reset(CreateTestWindow());
  w2->Show();
  wm::ActivateWindow(w2.get());
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_EQ(1.0f, w2->layer()->GetCombinedOpacity());
  EXPECT_FALSE(w1->IsVisible());

  // Close |w2|.
  w2.reset();
  EXPECT_EQ(1.0f, w1->layer()->GetCombinedOpacity());
  EXPECT_TRUE(w1->IsVisible());
}

// Verifies windows that are offscreen don't move when switching workspaces.
TEST_F(WorkspaceManager2Test, DontMoveOnSwitch) {
  aura::test::EventGenerator generator(
      Shell::GetPrimaryRootWindow(), gfx::Point());
  generator.MoveMouseTo(0, 0);

  scoped_ptr<Window> w1(CreateTestWindow());
  ShelfLayoutManager* shelf = Shell::GetInstance()->shelf();
  const gfx::Rect touches_shelf_bounds(
      0, shelf->GetIdealBounds().y() - 10, 101, 102);
  // Move |w1| to overlap the shelf.
  w1->SetBounds(touches_shelf_bounds);
  w1->Show();
  wm::ActivateWindow(w1.get());

  // Create another window and maximize it.
  scoped_ptr<Window> w2(CreateTestWindow());
  w2->SetBounds(gfx::Rect(10, 11, 250, 251));
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w2->Show();
  wm::ActivateWindow(w2.get());

  // Switch to w1.
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(touches_shelf_bounds.ToString(), w1->bounds().ToString());
}

// Verifies that windows that are completely offscreen move when switching
// workspaces.
TEST_F(WorkspaceManager2Test, MoveOnSwitch) {
  aura::test::EventGenerator generator(
      Shell::GetPrimaryRootWindow(), gfx::Point());
  generator.MoveMouseTo(0, 0);

  scoped_ptr<Window> w1(CreateTestWindow());
  ShelfLayoutManager* shelf = Shell::GetInstance()->shelf();
  const gfx::Rect w1_bounds(0, shelf->GetIdealBounds().y(), 100, 200);
  // Move |w1| so that the top edge is the same as the top edge of the shelf.
  w1->SetBounds(w1_bounds);
  w1->Show();
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(w1_bounds.ToString(), w1->bounds().ToString());

  // Create another window and maximize it.
  scoped_ptr<Window> w2(CreateTestWindow());
  w2->SetBounds(gfx::Rect(10, 11, 250, 251));
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w2->Show();
  wm::ActivateWindow(w2.get());

  // Increase the size of the shelf. This would make |w1| fall completely out of
  // the display work area.
  gfx::Size size = shelf->status()->GetWindowBoundsInScreen().size();
  size.Enlarge(0, 30);
  shelf->status()->SetSize(size);

  // Switch to w1. The window should have moved.
  wm::ActivateWindow(w1.get());
  EXPECT_NE(w1_bounds.ToString(), w1->bounds().ToString());
}

// Verifies Focus() works in a window that isn't in the active workspace.
TEST_F(WorkspaceManager2Test, FocusOnFullscreenInSeparateWorkspace) {
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->SetBounds(gfx::Rect(10, 11, 250, 251));
  w1->Show();
  wm::ActivateWindow(w1.get());

  scoped_ptr<Window> w2(CreateTestWindow());
  w2->SetBounds(gfx::Rect(10, 11, 250, 251));
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  w2->Show();
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_FALSE(wm::IsActiveWindow(w2.get()));

  w2->Focus();
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  EXPECT_FALSE(w1->IsVisible());
}

}  // namespace internal
}  // namespace ash
