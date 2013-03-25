// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_manager.h"

#include <map>

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shell_test_api.h"
#include "ash/wm/activation_controller.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace.h"
#include "ash/wm/workspace_controller_test_helper.h"
#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

using aura::Window;

namespace ash {
namespace internal {

// Returns a string containing the names of all the children of |window| (in
// order). Each entry is separated by a space.
std::string GetWindowNames(const aura::Window* window) {
  std::string result;
  for (size_t i = 0; i < window->children().size(); ++i) {
    if (i != 0)
      result += " ";
    result += window->children()[i]->name();
  }
  return result;
}

// Returns a string containing the names of windows corresponding to each of the
// child layers of |window|'s layer. Any layers that don't correspond to a child
// Window of |window| are ignored. The result is ordered based on the layer
// ordering.
std::string GetLayerNames(const aura::Window* window) {
  typedef std::map<const ui::Layer*, std::string> LayerToWindowNameMap;
  LayerToWindowNameMap window_names;
  for (size_t i = 0; i < window->children().size(); ++i) {
    window_names[window->children()[i]->layer()] =
        window->children()[i]->name();
  }

  std::string result;
  const std::vector<ui::Layer*>& layers(window->layer()->children());
  for (size_t i = 0; i < layers.size(); ++i) {
    LayerToWindowNameMap::iterator layer_i =
        window_names.find(layers[i]);
    if (layer_i != window_names.end()) {
      if (!result.empty())
        result += " ";
      result += layer_i->second;
    }
  }
  return result;
}

class WorkspaceManagerTest : public test::AshTestBase {
 public:
  WorkspaceManagerTest() : manager_(NULL) {}
  virtual ~WorkspaceManagerTest() {}

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
    SetDefaultParentByPrimaryRootWindow(window);
    return window;
  }

  aura::Window* GetViewport() {
    return Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                               kShellWindowId_DefaultContainer);
  }

  const std::vector<Workspace*>& workspaces() const {
    return manager_->workspaces_;
  }

  gfx::Rect GetFullscreenBounds(aura::Window* window) {
    return Shell::GetScreen()->GetDisplayNearestWindow(window).bounds();
  }

  Workspace* active_workspace() {
    return manager_->active_workspace_;
  }

  ShelfWidget* shelf_widget() {
    return Shell::GetPrimaryRootWindowController()->shelf();
  }

  ShelfLayoutManager* shelf_layout_manager() {
    return Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  }

  bool GetWindowOverlapsShelf() {
    return shelf_layout_manager()->window_overlaps_shelf();
  }

  Workspace* FindBy(aura::Window* window) const {
    return manager_->FindBy(window);
  }

  std::string WorkspaceStateString(Workspace* workspace) {
    return (workspace->is_maximized() ? "M" : "") +
        base::IntToString(static_cast<int>(
                              workspace->window()->children().size()));
  }

  int active_index() {
    return static_cast<int>(
        manager_->FindWorkspace(manager_->active_workspace_) -
        manager_->workspaces_.begin());
  }

  // Returns a string description of the current state. The string has the
  // following format:
  // W* P=W* active=N
  // Each W corresponds to a workspace. Each workspace is prefixed with an 'M'
  // if the workspace is maximized and is followed by the number of windows in
  // the workspace.
  // 'P=' is used for the pending workspaces (see
  // WorkspaceManager::pending_workspaces_ for details on pending workspaces).
  // N is the index of the active workspace (index into
  // WorkspaceManager::workspaces_).
  // For example, '2 M1 P=M1 active=1' means the first workspace (the desktop)
  // has 2 windows, the second workspace is a maximized workspace with 1 window,
  // there is a pending maximized workspace with 1 window and the second
  // workspace is active.
  std::string StateString() {
    std::string result;
    for (size_t i = 0; i < manager_->workspaces_.size(); ++i) {
      if (i > 0)
        result += " ";
      result += WorkspaceStateString(manager_->workspaces_[i]);
    }

    if (!manager_->pending_workspaces_.empty()) {
      result += " P=";
      for (std::set<Workspace*>::const_iterator i =
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
        test::ShellTestApi(Shell::GetInstance()).workspace_controller());
    manager_ = workspace_helper.workspace_manager();
  }

  virtual void TearDown() OVERRIDE {
    manager_ = NULL;
    test::AshTestBase::TearDown();
  }

 protected:
  WorkspaceManager* manager_;

 private:
  scoped_ptr<ActivationController> activation_controller_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceManagerTest);
};

// Assertions around adding a normal window.
TEST_F(WorkspaceManagerTest, AddNormalWindowWhenEmpty) {
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
TEST_F(WorkspaceManagerTest, SingleMaximizeWindow) {
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
TEST_F(WorkspaceManagerTest, CloseLastWindowInWorkspace) {
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
TEST_F(WorkspaceManagerTest, AddMaximizedWindowWhenEmpty) {
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
TEST_F(WorkspaceManagerTest, MaximizeWithNormalWindow) {
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
TEST_F(WorkspaceManagerTest, TwoMaximized) {
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
TEST_F(WorkspaceManagerTest, ChangeBoundsOfNormalWindow) {
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->Show();

  // Setting the bounds should go through since the window is in the normal
  // workspace.
  w1->SetBounds(gfx::Rect(0, 0, 200, 500));
  EXPECT_EQ(200, w1->bounds().width());
  EXPECT_EQ(500, w1->bounds().height());
}

// Verifies the bounds is not altered when showing and grid is enabled.
TEST_F(WorkspaceManagerTest, SnapToGrid) {
  scoped_ptr<Window> w1(CreateTestWindowUnparented());
  w1->SetBounds(gfx::Rect(1, 6, 25, 30));
  SetDefaultParentByPrimaryRootWindow(w1.get());
  // We are not aligning this anymore this way. When the window gets shown
  // the window is expected to be handled differently, but this cannot be
  // tested with this test. So the result of this test should be that the
  // bounds are exactly as passed in.
  EXPECT_EQ("1,6 25x30", w1->bounds().ToString());
}

// Assertions around a fullscreen window.
TEST_F(WorkspaceManagerTest, SingleFullscreenWindow) {
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
TEST_F(WorkspaceManagerTest, DontShowTransientsOnSwitch) {
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
TEST_F(WorkspaceManagerTest, MinimizeSingleWindow) {
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
TEST_F(WorkspaceManagerTest, MinimizeMaximizedWindow) {
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
TEST_F(WorkspaceManagerTest, ShelfStateUpdated) {
  // Since ShelfLayoutManager queries for mouse location, move the mouse so
  // it isn't over the shelf.
  aura::test::EventGenerator generator(
      Shell::GetPrimaryRootWindow(), gfx::Point());
  generator.MoveMouseTo(0, 0);

  scoped_ptr<Window> w1(CreateTestWindow());
  const gfx::Rect w1_bounds(0, 1, 101, 102);
  ShelfLayoutManager* shelf = shelf_layout_manager();
  shelf->SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
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

  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());

  // Maximize the window.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Restore.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());

  // Fullscreen.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());

  // Normal.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
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
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Minimize.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());

  // Since the restore from minimize will restore to the pre-minimize
  // state (tested elsewhere), we abandon the current size and restore
  // rect and set them to the window.
  gfx::Rect restore = *GetRestoreBoundsInScreen(w1.get());
  EXPECT_EQ("0,0 800x597", w1->bounds().ToString());
  EXPECT_EQ("0,1 101x102", restore.ToString());
  ClearRestoreBounds(w1.get());
  w1->SetBounds(restore);

  // Restore.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());

  // Create another window, maximized.
  scoped_ptr<Window> w2(CreateTestWindow());
  w2->SetBounds(gfx::Rect(10, 11, 250, 251));
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w2->Show();
  wm::ActivateWindow(w2.get());
  EXPECT_EQ(1, active_index());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());

  // Switch to w1.
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(0, active_index());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ("0,1 101x102", w1->bounds().ToString());
  EXPECT_EQ(ScreenAsh::GetMaximizedWindowBoundsInParent(
                w2->parent()).ToString(),
            w2->bounds().ToString());

  // Switch to w2.
  wm::ActivateWindow(w2.get());
  EXPECT_EQ(1, active_index());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
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
TEST_F(WorkspaceManagerTest, PersistAcrossAllWorkspaces) {
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
TEST_F(WorkspaceManagerTest, ActivatePersistAcrossAllWorkspacesWhenNotActive) {
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
TEST_F(WorkspaceManagerTest, ShowMinimizedPersistWindow) {
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

// Test that a persistent window across all workspaces which was first
// maximized, then got minimized and finally got restored does not crash the
// system (see http://crbug.com/151698) and restores its maximized workspace
// instead.
TEST_F(WorkspaceManagerTest, MaximizeMinimizeRestoreDoesNotCrash) {
  // We need to create a regular window first so there's an active workspace.
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->Show();

  // Create a window that persists across all workspaces.
  scoped_ptr<Window> w2(CreateTestWindow());
  SetPersistsAcrossAllWorkspaces(
      w2.get(),
      WINDOW_PERSISTS_ACROSS_ALL_WORKSPACES_VALUE_YES);
  w2->Show();
  wm::ActivateWindow(w2.get());
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_FALSE(w2->IsVisible());
  // This is the critical call which should switch to the maximized workspace
  // of that window instead of reparenting it to the other workspace (and
  // crashing while trying to do so).
  wm::ActivateWindow(w2.get());
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED,
            w2->GetProperty(aura::client::kShowStateKey));
  EXPECT_TRUE(w2->IsVisible());
}

// Test that we report we're in the fullscreen state even if the fullscreen
// window isn't being managed by us (http://crbug.com/123931).
TEST_F(WorkspaceManagerTest, GetWindowStateWithUnmanagedFullscreenWindow) {
  ShelfLayoutManager* shelf = shelf_layout_manager();

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

  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());
  EXPECT_EQ(WORKSPACE_WINDOW_STATE_FULL_SCREEN, manager_->GetWindowState());

  w2->Hide();
  ASSERT_EQ("1 P=M1 active=0", StateString());
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  w2->Show();
  ASSERT_EQ("1 P=M1 active=0", StateString());
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_EQ(WORKSPACE_WINDOW_STATE_DEFAULT, manager_->GetWindowState());

  wm::ActivateWindow(w2.get());
  ASSERT_EQ("1 M1 active=1", StateString());
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());
  EXPECT_EQ(WORKSPACE_WINDOW_STATE_FULL_SCREEN, manager_->GetWindowState());

  w2.reset();
  ASSERT_EQ("1 active=0", StateString());
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_EQ(WORKSPACE_WINDOW_STATE_DEFAULT, manager_->GetWindowState());
}

// Variant of GetWindowStateWithUnmanagedFullscreenWindow that uses a maximized
// window rather than a normal window.
TEST_F(WorkspaceManagerTest,
       GetWindowStateWithUnmanagedFullscreenWindowWithMaximized) {
  ShelfLayoutManager* shelf = shelf_layout_manager();
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
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());
  EXPECT_EQ(WORKSPACE_WINDOW_STATE_FULL_SCREEN,
            manager_->GetWindowState());

  w2->Hide();
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  w2->Show();
  wm::ActivateWindow(w2.get());
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());
  EXPECT_EQ(WORKSPACE_WINDOW_STATE_FULL_SCREEN,
            manager_->GetWindowState());

  w2.reset();
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
}

// Verifies a window marked as persisting across all workspaces ends up in its
// own workspace when maximized.
TEST_F(WorkspaceManagerTest, MaximizeDontPersistEndsUpInOwnWorkspace) {
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
TEST_F(WorkspaceManagerTest, MinimizeResetsVisibility) {
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(SHELF_VISIBLE,
            shelf_layout_manager()->visibility_state());
  EXPECT_FALSE(shelf_widget()->paints_background());
}

// Verifies transients are moved when maximizing.
TEST_F(WorkspaceManagerTest, MoveTransientOnMaximize) {
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
  SetDefaultParentByPrimaryRootWindow(w3.get());
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
TEST_F(WorkspaceManagerTest, VisibilityTests) {
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
TEST_F(WorkspaceManagerTest, DontMoveOnSwitch) {
  aura::test::EventGenerator generator(
      Shell::GetPrimaryRootWindow(), gfx::Point());
  generator.MoveMouseTo(0, 0);

  scoped_ptr<Window> w1(CreateTestWindow());
  ShelfLayoutManager* shelf = shelf_layout_manager();
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
TEST_F(WorkspaceManagerTest, MoveOnSwitch) {
  aura::test::EventGenerator generator(
      Shell::GetPrimaryRootWindow(), gfx::Point());
  generator.MoveMouseTo(0, 0);

  scoped_ptr<Window> w1(CreateTestWindow());
  ShelfLayoutManager* shelf = shelf_layout_manager();
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
  gfx::Size size(shelf_widget()->status_area_widget()->
      GetWindowBoundsInScreen().size());
  size.Enlarge(0, 30);
  shelf_widget()->status_area_widget()->SetSize(size);

  // Switch to w1. The window should have moved.
  wm::ActivateWindow(w1.get());
  EXPECT_NE(w1_bounds.ToString(), w1->bounds().ToString());
}

// Verifies Focus() works in a window that isn't in the active workspace.
TEST_F(WorkspaceManagerTest, FocusOnFullscreenInSeparateWorkspace) {
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

namespace {

// WindowDelegate used by DontCrashOnChangeAndActivate.
class DontCrashOnChangeAndActivateDelegate
    : public aura::test::TestWindowDelegate {
 public:
  DontCrashOnChangeAndActivateDelegate() : window_(NULL) {}

  void set_window(aura::Window* window) { window_ = window; }

  // WindowDelegate overrides:
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {
    if (window_) {
      wm::ActivateWindow(window_);
      window_ = NULL;
    }
  }

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(DontCrashOnChangeAndActivateDelegate);
};

}  // namespace

// Exercises possible crash in W2. Here's the sequence:
// . minimize a maximized window.
// . remove the window (which happens when switching displays).
// . add the window back.
// . show the window and during the bounds change activate it.
TEST_F(WorkspaceManagerTest, DontCrashOnChangeAndActivate) {
  // Force the shelf
  ShelfLayoutManager* shelf = shelf_layout_manager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

  DontCrashOnChangeAndActivateDelegate delegate;
  scoped_ptr<Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate, 1000, gfx::Rect(10, 11, 250, 251)));

  w1->Show();
  wm::ActivateWindow(w1.get());
  wm::MaximizeWindow(w1.get());
  wm::MinimizeWindow(w1.get());

  w1->parent()->RemoveChild(w1.get());

  // Do this so that when we Show() the window a resize occurs and we make the
  // window active.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  SetDefaultParentByPrimaryRootWindow(w1.get());
  delegate.set_window(w1.get());
  w1->Show();
}

// Verifies a window with a transient parent not managed by workspace works.
TEST_F(WorkspaceManagerTest, TransientParent) {
  // Normal window with no transient parent.
  scoped_ptr<Window> w2(CreateTestWindow());
  w2->SetBounds(gfx::Rect(10, 11, 250, 251));
  w2->Show();
  wm::ActivateWindow(w2.get());

  // Window with a transient parent. We set the transient parent to the root,
  // which would never happen but is enough to exercise the bug.
  scoped_ptr<Window> w1(CreateTestWindowUnparented());
  Shell::GetInstance()->GetPrimaryRootWindow()->AddTransientChild(w1.get());
  w1->SetBounds(gfx::Rect(10, 11, 250, 251));
  SetDefaultParentByPrimaryRootWindow(w1.get());
  w1->Show();
  wm::ActivateWindow(w1.get());

  // The window with the transient parent should get added to the same parent as
  // the normal window.
  EXPECT_EQ(w2->parent(), w1->parent());
}

// Verifies changing TrackedByWorkspace works.
TEST_F(WorkspaceManagerTest, TrackedByWorkspace) {
  // Create a window maximized.
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_TRUE(w1->IsVisible());

  // Create a second window maximized and mark it not tracked by workspace
  // manager.
  scoped_ptr<Window> w2(CreateTestWindowUnparented());
  w2->SetBounds(gfx::Rect(1, 6, 25, 30));
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  SetDefaultParentByPrimaryRootWindow(w2.get());
  w2->Show();
  SetTrackedByWorkspace(w2.get(), false);
  wm::ActivateWindow(w2.get());

  // Activating |w2| should force it to have the same parent as |w1|.
  EXPECT_EQ(w1->parent(), w2->parent());
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());

  // Because |w2| isn't tracked we should be able to set the bounds of it.
  gfx::Rect bounds(w2->bounds());
  bounds.Offset(4, 5);
  w2->SetBounds(bounds);
  EXPECT_EQ(bounds.ToString(), w2->bounds().ToString());

  // Transition it to tracked by worskpace. It should end up in its own
  // workspace.
  SetTrackedByWorkspace(w2.get(), true);
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  EXPECT_FALSE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_NE(w1->parent(), w2->parent());
}

// Verifies a window marked as persisting across all workspaces ends up in its
// own workspace when maximized.
TEST_F(WorkspaceManagerTest, DeactivateDropsToDesktop) {
  // Create a window maximized.
  scoped_ptr<Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_TRUE(w1->IsVisible());

  // Create another window that persists across all workspaces. It should end
  // up with the same parent as |w1|.
  scoped_ptr<Window> w2(CreateTestWindow());
  SetPersistsAcrossAllWorkspaces(
      w2.get(),
      WINDOW_PERSISTS_ACROSS_ALL_WORKSPACES_VALUE_YES);
  w2->Show();
  wm::ActivateWindow(w2.get());
  EXPECT_EQ(w1->parent(), w2->parent());
  ASSERT_EQ("0 M2 active=1", StateString());

  // Activate |w1|, should result in dropping |w2| to the desktop.
  wm::ActivateWindow(w1.get());
  ASSERT_EQ("1 M1 active=1", StateString());
}

// Test the basic auto placement of one and or two windows in a "simulated
// session" of sequential window operations.
TEST_F(WorkspaceManagerTest, BasicAutoPlacing) {
  // Test 1: In case there is no manageable window, no window should shift.

  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  window1->SetBounds(gfx::Rect(16, 32, 640, 320));
  gfx::Rect desktop_area = window1->parent()->bounds();

  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  // Trigger the auto window placement function by making it visible.
  // Note that the bounds are getting changed while it is invisible.
  window2->Hide();
  window2->SetBounds(gfx::Rect(32, 48, 256, 512));
  window2->Show();

  // Check the initial position of the windows is unchanged.
  EXPECT_EQ("16,32 640x320", window1->bounds().ToString());
  EXPECT_EQ("32,48 256x512", window2->bounds().ToString());

  // Remove the second window and make sure that the first window
  // does NOT get centered.
  window2.reset();
  EXPECT_EQ("16,32 640x320", window1->bounds().ToString());

  // Test 2: Set up two managed windows and check their auto positioning.
  ash::wm::SetWindowPositionManaged(window1.get(), true);
  scoped_ptr<aura::Window> window3(CreateTestWindowInShellWithId(2));
  ash::wm::SetWindowPositionManaged(window3.get(), true);
  // To avoid any auto window manager changes due to SetBounds, the window
  // gets first hidden and then shown again.
  window3->Hide();
  window3->SetBounds(gfx::Rect(32, 48, 256, 512));
  window3->Show();
  // |window1| should be flush right and |window3| flush left.
  EXPECT_EQ("0,32 640x320", window1->bounds().ToString());
  EXPECT_EQ(base::IntToString(
                desktop_area.width() - window3->bounds().width()) +
            ",48 256x512", window3->bounds().ToString());

  // After removing |window3|, |window1| should be centered again.
  window3.reset();
  EXPECT_EQ(
      base::IntToString(
          (desktop_area.width() - window1->bounds().width()) / 2) +
      ",32 640x320", window1->bounds().ToString());

  // Test 3: Set up a manageable and a non manageable window and check
  // positioning.
  scoped_ptr<aura::Window> window4(CreateTestWindowInShellWithId(3));
  // To avoid any auto window manager changes due to SetBounds, the window
  // gets first hidden and then shown again.
  window1->Hide();
  window1->SetBounds(gfx::Rect(16, 32, 640, 320));
  window4->SetBounds(gfx::Rect(32, 48, 256, 512));
  window1->Show();
  // |window1| should be centered and |window4| untouched.
  EXPECT_EQ(
      base::IntToString(
          (desktop_area.width() - window1->bounds().width()) / 2) +
      ",32 640x320", window1->bounds().ToString());
  EXPECT_EQ("32,48 256x512", window4->bounds().ToString());

  // Test4: A single manageable window should get centered.
  window4.reset();
  ash::wm::SetUserHasChangedWindowPositionOrSize(window1.get(), false);
  // Trigger the auto window placement function by showing (and hiding) it.
  window1->Hide();
  window1->Show();
  // |window1| should be centered.
  EXPECT_EQ(
      base::IntToString(
          (desktop_area.width() - window1->bounds().width()) / 2) +
      ",32 640x320", window1->bounds().ToString());
}

// Test the proper usage of user window movement interaction.
TEST_F(WorkspaceManagerTest, TestUserMovedWindowRepositioning) {
  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  window1->SetBounds(gfx::Rect(16, 32, 640, 320));
  gfx::Rect desktop_area = window1->parent()->bounds();
  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  window2->SetBounds(gfx::Rect(32, 48, 256, 512));
  window1->Hide();
  window2->Hide();
  ash::wm::SetWindowPositionManaged(window1.get(), true);
  ash::wm::SetWindowPositionManaged(window2.get(), true);
  EXPECT_FALSE(ash::wm::HasUserChangedWindowPositionOrSize(window1.get()));
  EXPECT_FALSE(ash::wm::HasUserChangedWindowPositionOrSize(window2.get()));

  // Check that the current location gets preserved if the user has
  // positioned it previously.
  ash::wm::SetUserHasChangedWindowPositionOrSize(window1.get(), true);
  window1->Show();
  EXPECT_EQ("16,32 640x320", window1->bounds().ToString());
  // Flag should be still set.
  EXPECT_TRUE(ash::wm::HasUserChangedWindowPositionOrSize(window1.get()));
  EXPECT_FALSE(ash::wm::HasUserChangedWindowPositionOrSize(window2.get()));

  // Turn on the second window and make sure that both windows are now
  // positionable again (user movement cleared).
  window2->Show();

  // |window1| should be flush left and |window3| flush right.
  EXPECT_EQ("0,32 640x320", window1->bounds().ToString());
  EXPECT_EQ(
      base::IntToString(desktop_area.width() - window2->bounds().width()) +
      ",48 256x512", window2->bounds().ToString());
  // FLag should now be reset.
  EXPECT_FALSE(ash::wm::HasUserChangedWindowPositionOrSize(window1.get()));
  EXPECT_FALSE(ash::wm::HasUserChangedWindowPositionOrSize(window1.get()));

  // Going back to one shown window should keep the state.
  ash::wm::SetUserHasChangedWindowPositionOrSize(window1.get(), true);
  window2->Hide();
  EXPECT_EQ("0,32 640x320", window1->bounds().ToString());
  EXPECT_TRUE(ash::wm::HasUserChangedWindowPositionOrSize(window1.get()));
}

// Test that user placed windows go back to their user placement after the user
// closes all other windows.
TEST_F(WorkspaceManagerTest, TestUserHandledWindowRestore) {
  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  gfx::Rect user_pos = gfx::Rect(16, 42, 640, 320);
  window1->SetBounds(user_pos);
  ash::wm::SetPreAutoManageWindowBounds(window1.get(), user_pos);
  gfx::Rect desktop_area = window1->parent()->bounds();

  // Create a second window to let the auto manager kick in.
  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  window2->SetBounds(gfx::Rect(32, 48, 256, 512));
  window1->Hide();
  window2->Hide();
  ash::wm::SetWindowPositionManaged(window1.get(), true);
  ash::wm::SetWindowPositionManaged(window2.get(), true);
  window1->Show();
  EXPECT_EQ(user_pos.ToString(), window1->bounds().ToString());
  window2->Show();

  // |window1| should be flush left and |window2| flush right.
  EXPECT_EQ("0," + base::IntToString(user_pos.y()) +
            " 640x320", window1->bounds().ToString());
  EXPECT_EQ(
      base::IntToString(desktop_area.width() - window2->bounds().width()) +
      ",48 256x512", window2->bounds().ToString());
  window2->Hide();

  // After the other window get hidden the window has to move back to the
  // previous position and the bounds should still be set and unchanged.
  EXPECT_EQ(user_pos.ToString(), window1->bounds().ToString());
  ASSERT_TRUE(ash::wm::GetPreAutoManageWindowBounds(window1.get()));
  EXPECT_EQ(user_pos.ToString(),
            ash::wm::GetPreAutoManageWindowBounds(window1.get())->ToString());
}

// Test that a window from normal to minimize will repos the remaining.
TEST_F(WorkspaceManagerTest, ToMinimizeRepositionsRemaining) {
  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  ash::wm::SetWindowPositionManaged(window1.get(), true);
  window1->SetBounds(gfx::Rect(16, 32, 640, 320));
  gfx::Rect desktop_area = window1->parent()->bounds();

  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  ash::wm::SetWindowPositionManaged(window2.get(), true);
  window2->SetBounds(gfx::Rect(32, 48, 256, 512));

  ash::wm::MinimizeWindow(window1.get());

  // |window2| should be centered now.
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(ash::wm::IsWindowNormal(window2.get()));
  EXPECT_EQ(base::IntToString(
                (desktop_area.width() - window2->bounds().width()) / 2) +
            ",48 256x512", window2->bounds().ToString());

  ash::wm::RestoreWindow(window1.get());
  // |window1| should be flush right and |window3| flush left.
  EXPECT_EQ(base::IntToString(
                desktop_area.width() - window1->bounds().width()) +
            ",32 640x320", window1->bounds().ToString());
  EXPECT_EQ("0,48 256x512", window2->bounds().ToString());
}

// Test that minimizing an initially maximized window will repos the remaining.
TEST_F(WorkspaceManagerTest, MaxToMinRepositionsRemaining) {
  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  ash::wm::SetWindowPositionManaged(window1.get(), true);
  gfx::Rect desktop_area = window1->parent()->bounds();

  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  ash::wm::SetWindowPositionManaged(window2.get(), true);
  window2->SetBounds(gfx::Rect(32, 48, 256, 512));

  ash::wm::MaximizeWindow(window1.get());
  ash::wm::MinimizeWindow(window1.get());

  // |window2| should be centered now.
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(ash::wm::IsWindowNormal(window2.get()));
  EXPECT_EQ(base::IntToString(
                (desktop_area.width() - window2->bounds().width()) / 2) +
            ",48 256x512", window2->bounds().ToString());
}

// Test that nomral, maximize, minimizing will repos the remaining.
TEST_F(WorkspaceManagerTest, NormToMaxToMinRepositionsRemaining) {
  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  window1->SetBounds(gfx::Rect(16, 32, 640, 320));
  ash::wm::SetWindowPositionManaged(window1.get(), true);
  gfx::Rect desktop_area = window1->parent()->bounds();

  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  ash::wm::SetWindowPositionManaged(window2.get(), true);
  window2->SetBounds(gfx::Rect(32, 40, 256, 512));

  // Trigger the auto window placement function by showing (and hiding) it.
  window1->Hide();
  window1->Show();

  // |window1| should be flush right and |window3| flush left.
  EXPECT_EQ(base::IntToString(
                desktop_area.width() - window1->bounds().width()) +
            ",32 640x320", window1->bounds().ToString());
  EXPECT_EQ("0,40 256x512", window2->bounds().ToString());

  ash::wm::MaximizeWindow(window1.get());
  ash::wm::MinimizeWindow(window1.get());

  // |window2| should be centered now.
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(ash::wm::IsWindowNormal(window2.get()));
  EXPECT_EQ(base::IntToString(
                (desktop_area.width() - window2->bounds().width()) / 2) +
            ",40 256x512", window2->bounds().ToString());
}

// Test that nomral, maximize, normal will repos the remaining.
TEST_F(WorkspaceManagerTest, NormToMaxToNormRepositionsRemaining) {
  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  window1->SetBounds(gfx::Rect(16, 32, 640, 320));
  ash::wm::SetWindowPositionManaged(window1.get(), true);
  gfx::Rect desktop_area = window1->parent()->bounds();

  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  ash::wm::SetWindowPositionManaged(window2.get(), true);
  window2->SetBounds(gfx::Rect(32, 40, 256, 512));

  // Trigger the auto window placement function by showing (and hiding) it.
  window1->Hide();
  window1->Show();

  // |window1| should be flush right and |window3| flush left.
  EXPECT_EQ(base::IntToString(
                desktop_area.width() - window1->bounds().width()) +
            ",32 640x320", window1->bounds().ToString());
  EXPECT_EQ("0,40 256x512", window2->bounds().ToString());

  ash::wm::MaximizeWindow(window1.get());
  ash::wm::RestoreWindow(window1.get());

  // |window1| should be flush right and |window2| flush left.
  EXPECT_EQ(base::IntToString(
                desktop_area.width() - window1->bounds().width()) +
            ",32 640x320", window1->bounds().ToString());
  EXPECT_EQ("0,40 256x512", window2->bounds().ToString());
}

// Test that animations are triggered.
TEST_F(WorkspaceManagerTest, AnimatedNormToMaxToNormRepositionsRemaining) {
  ui::ScopedAnimationDurationScaleMode normal_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  window1->Hide();
  window1->SetBounds(gfx::Rect(16, 32, 640, 320));
  gfx::Rect desktop_area = window1->parent()->bounds();
  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  window2->Hide();
  window2->SetBounds(gfx::Rect(32, 48, 256, 512));

  ash::wm::SetWindowPositionManaged(window1.get(), true);
  ash::wm::SetWindowPositionManaged(window2.get(), true);
  // Make sure nothing is animating.
  window1->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();
  window2->Show();

  // The second window should now animate.
  EXPECT_FALSE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window2->layer()->GetAnimator()->is_animating());
  window2->layer()->GetAnimator()->StopAnimating();

  window1->Show();
  EXPECT_TRUE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window2->layer()->GetAnimator()->is_animating());

  window1->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();
  // |window1| should be flush right and |window2| flush left.
  EXPECT_EQ(base::IntToString(
                desktop_area.width() - window1->bounds().width()) +
            ",32 640x320", window1->bounds().ToString());
  EXPECT_EQ("0,48 256x512", window2->bounds().ToString());
}

// This tests simulates a browser and an app and verifies the ordering of the
// windows and layers doesn't get out of sync as various operations occur. Its
// really testing code in FocusController, but easier to simulate here. Just as
// with a real browser the browser here has a transient child window
// (corresponds to the status bubble).
TEST_F(WorkspaceManagerTest, VerifyLayerOrdering) {
  scoped_ptr<Window> browser(
      aura::test::CreateTestWindowWithDelegate(
          NULL,
          aura::client::WINDOW_TYPE_NORMAL,
          gfx::Rect(5, 6, 7, 8),
          NULL));
  browser->SetName("browser");
  SetDefaultParentByPrimaryRootWindow(browser.get());
  browser->Show();
  wm::ActivateWindow(browser.get());

  // |status_bubble| is made a transient child of |browser| and as a result
  // owned by |browser|.
  aura::test::TestWindowDelegate* status_bubble_delegate =
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();
  status_bubble_delegate->set_can_focus(false);
  Window* status_bubble =
      aura::test::CreateTestWindowWithDelegate(
          status_bubble_delegate,
          aura::client::WINDOW_TYPE_POPUP,
          gfx::Rect(5, 6, 7, 8),
          NULL);
  browser->AddTransientChild(status_bubble);
  SetDefaultParentByPrimaryRootWindow(status_bubble);
  status_bubble->SetName("status_bubble");

  scoped_ptr<Window> app(
      aura::test::CreateTestWindowWithDelegate(
          NULL,
          aura::client::WINDOW_TYPE_NORMAL,
          gfx::Rect(5, 6, 7, 8),
          NULL));
  app->SetName("app");
  SetDefaultParentByPrimaryRootWindow(app.get());

  aura::Window* parent = browser->parent();

  app->Show();
  wm::ActivateWindow(app.get());
  EXPECT_EQ(GetWindowNames(parent), GetLayerNames(parent));

  // Minimize the app, focus should go the browser.
  app->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_TRUE(wm::IsActiveWindow(browser.get()));
  EXPECT_EQ(GetWindowNames(parent), GetLayerNames(parent));

  // Minimize the browser (neither windows are focused).
  browser->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_FALSE(wm::IsActiveWindow(browser.get()));
  EXPECT_FALSE(wm::IsActiveWindow(app.get()));
  EXPECT_EQ(GetWindowNames(parent), GetLayerNames(parent));

  // Show the browser (which should restore it).
  browser->Show();
  EXPECT_EQ(GetWindowNames(parent), GetLayerNames(parent));

  // Activate the browser.
  ash::wm::ActivateWindow(browser.get());
  EXPECT_TRUE(wm::IsActiveWindow(browser.get()));
  EXPECT_EQ(GetWindowNames(parent), GetLayerNames(parent));

  // Restore the app. This differs from above code for |browser| as internally
  // the app code does this. Restoring this way or using Show() should not make
  // a difference.
  app->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(GetWindowNames(parent), GetLayerNames(parent));

  // Activate the app.
  ash::wm::ActivateWindow(app.get());
  EXPECT_TRUE(wm::IsActiveWindow(app.get()));
  EXPECT_EQ(GetWindowNames(parent), GetLayerNames(parent));
}

namespace {

// Used by DragMaximizedNonTrackedWindow to track how many times the window
// hierarchy changes.
class DragMaximizedNonTrackedWindowObserver
    : public aura::WindowObserver {
 public:
  DragMaximizedNonTrackedWindowObserver() : change_count_(0) {
  }

  // Number of times OnWindowHierarchyChanged() has been received.
  void clear_change_count() { change_count_ = 0; }
  int change_count() const {
    return change_count_;
  }

  // aura::WindowObserver overrides:
  virtual void OnWindowHierarchyChanged(
      const HierarchyChangeParams& params) OVERRIDE {
    change_count_++;
  }

 private:
  int change_count_;

  DISALLOW_COPY_AND_ASSIGN(DragMaximizedNonTrackedWindowObserver);
};

}  // namespace

// Verifies setting tracked by workspace to false and then dragging a maximized
// window doesn't result in changing the window hierarchy (which typically
// indicates new workspaces have been created).
TEST_F(WorkspaceManagerTest, DragMaximizedNonTrackedWindow) {
  aura::test::EventGenerator generator(
      Shell::GetPrimaryRootWindow(), gfx::Point());
  generator.MoveMouseTo(5, 5);

  aura::test::TestWindowDelegate delegate;
  delegate.set_window_component(HTCAPTION);
  scoped_ptr<Window> w1(
      aura::test::CreateTestWindowWithDelegate(&delegate,
                                               aura::client::WINDOW_TYPE_NORMAL,
                                               gfx::Rect(5, 6, 7, 8),
                                               NULL));
  SetDefaultParentByPrimaryRootWindow(w1.get());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  w1->Show();
  wm::ActivateWindow(w1.get());
  DragMaximizedNonTrackedWindowObserver observer;
  w1->parent()->parent()->AddObserver(&observer);
  const gfx::Rect max_bounds(w1->bounds());

  // There should be two workspace, one for the desktop and one for the
  // maximized window with the maximized active.
  EXPECT_EQ("0 M1 active=1", StateString());

  generator.PressLeftButton();
  generator.MoveMouseTo(100, 100);
  // The bounds shouldn't change (drag should result in nothing happening
  // now.
  EXPECT_EQ(max_bounds.ToString(), w1->bounds().ToString());
  EXPECT_EQ("0 M1 active=1", StateString());

  generator.ReleaseLeftButton();
  EXPECT_EQ(0, observer.change_count());

  // Set tracked to false and repeat, now the window should move.
  SetTrackedByWorkspace(w1.get(), false);
  generator.MoveMouseTo(5, 5);
  generator.PressLeftButton();
  generator.MoveMouseBy(100, 100);
  EXPECT_EQ(gfx::Rect(max_bounds.x() + 100, max_bounds.y() + 100,
                      max_bounds.width(), max_bounds.height()).ToString(),
            w1->bounds().ToString());
  EXPECT_EQ("0 M1 active=1", StateString());

  generator.ReleaseLeftButton();
  SetTrackedByWorkspace(w1.get(), true);
  // Marking the window tracked again should snap back to origin.
  EXPECT_EQ("0 M1 active=1", StateString());
  EXPECT_EQ(max_bounds.ToString(), w1->bounds().ToString());
  EXPECT_EQ(0, observer.change_count());

  w1->parent()->parent()->RemoveObserver(&observer);
}

}  // namespace internal
}  // namespace ash
