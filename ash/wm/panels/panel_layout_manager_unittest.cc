// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panels/panel_layout_manager.h"

#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_button.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_view.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/launcher_view_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_launcher_delegate.h"
#include "ash/wm/window_util.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/views/corewm/corewm_switches.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

using aura::test::WindowIsAbove;

class PanelLayoutManagerTest : public test::AshTestBase {
 public:
  PanelLayoutManagerTest() {}
  virtual ~PanelLayoutManagerTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    ASSERT_TRUE(test::TestLauncherDelegate::instance());

    launcher_view_test_.reset(new test::LauncherViewTestAPI(
        Launcher::ForPrimaryDisplay()->GetLauncherViewForTest()));
    launcher_view_test_->SetAnimationDuration(1);
  }

  aura::Window* CreateNormalWindow() {
    return CreateTestWindowInShellWithBounds(gfx::Rect());
  }

  aura::Window* CreatePanelWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        NULL,
        aura::client::WINDOW_TYPE_PANEL,
        0,
        bounds);
    test::TestLauncherDelegate* launcher_delegate =
        test::TestLauncherDelegate::instance();
    launcher_delegate->AddLauncherItem(window);
    PanelLayoutManager* manager =
        static_cast<PanelLayoutManager*>(GetPanelContainer()->layout_manager());
    manager->Relayout();
    return window;
  }

  aura::Window* GetPanelContainer() {
    return Shell::GetContainer(
        Shell::GetPrimaryRootWindow(),
        internal::kShellWindowId_PanelContainer);
  }

  views::Widget* GetCalloutWidgetForPanel(aura::Window* panel) {
    PanelLayoutManager* manager =
        static_cast<PanelLayoutManager*>(GetPanelContainer()->layout_manager());
    DCHECK(manager);
    PanelLayoutManager::PanelList::iterator found = std::find(
        manager->panel_windows_.begin(), manager->panel_windows_.end(),
        panel);
    DCHECK(found != manager->panel_windows_.end());
    DCHECK(found->callout_widget);
    return reinterpret_cast<views::Widget*>(found->callout_widget);
  }

  void PanelInScreen(aura::Window* panel) {
    gfx::Rect panel_bounds = panel->GetBoundsInRootWindow();
    gfx::Point root_point = gfx::Point(panel_bounds.x(), panel_bounds.y());
    gfx::Display display = ScreenAsh::FindDisplayContainingPoint(root_point);

    gfx::Rect panel_bounds_in_screen = panel->GetBoundsInScreen();
    gfx::Point screen_bottom_right = gfx::Point(
        panel_bounds_in_screen.right(),
        panel_bounds_in_screen.bottom());
    gfx::Rect display_bounds = display.bounds();
    EXPECT_TRUE(screen_bottom_right.x() < display_bounds.width() &&
                screen_bottom_right.y() < display_bounds.height());
  }

  void PanelsNotOverlapping(aura::Window* panel1, aura::Window* panel2) {
    // Waits until all launcher view animations are done.
    launcher_view_test()->RunMessageLoopUntilAnimationsDone();
    gfx::Rect window1_bounds = panel1->GetBoundsInRootWindow();
    gfx::Rect window2_bounds = panel2->GetBoundsInRootWindow();

    EXPECT_FALSE(window1_bounds.Intersects(window2_bounds));
  }

  // TODO(dcheng): This should be const, but GetScreenBoundsOfItemIconForWindow
  // takes a non-const Window. We can probably fix that.
  void IsPanelAboveLauncherIcon(aura::Window* panel) {
    // Waits until all launcher view animations are done.
    launcher_view_test()->RunMessageLoopUntilAnimationsDone();

    Launcher* launcher = Launcher::ForPrimaryDisplay();
    gfx::Rect icon_bounds = launcher->GetScreenBoundsOfItemIconForWindow(panel);
    ASSERT_FALSE(icon_bounds.IsEmpty());

    gfx::Rect window_bounds = panel->GetBoundsInRootWindow();
    gfx::Rect launcher_bounds = launcher->shelf_widget()->
        GetWindowBoundsInScreen();
    ShelfAlignment alignment = GetAlignment();

    if (IsHorizontal(alignment)) {
      // The horizontal bounds of the panel window should contain the bounds of
      // the launcher icon.
      EXPECT_LE(window_bounds.x(), icon_bounds.x());
      EXPECT_GE(window_bounds.right(), icon_bounds.right());
    } else {
      // The vertical bounds of the panel window should contain the bounds of
      // the launcher icon.
      EXPECT_LE(window_bounds.y(), icon_bounds.y());
      EXPECT_GE(window_bounds.bottom(), icon_bounds.bottom());
    }

    switch (alignment) {
      case SHELF_ALIGNMENT_BOTTOM:
        EXPECT_EQ(launcher_bounds.y(), window_bounds.bottom());
        break;
      case SHELF_ALIGNMENT_LEFT:
        EXPECT_EQ(launcher_bounds.right(), window_bounds.x());
        break;
      case SHELF_ALIGNMENT_RIGHT:
        EXPECT_EQ(launcher_bounds.x(), window_bounds.right());
        break;
      case SHELF_ALIGNMENT_TOP:
        EXPECT_EQ(launcher_bounds.bottom(), window_bounds.y());
        break;
    }
  }

  void IsCalloutAboveLauncherIcon(aura::Window* panel) {
    // Flush the message loop, since callout updates use a delayed task.
    base::RunLoop().RunUntilIdle();
    views::Widget* widget = GetCalloutWidgetForPanel(panel);

    Launcher* launcher = Launcher::ForPrimaryDisplay();
    gfx::Rect icon_bounds = launcher->GetScreenBoundsOfItemIconForWindow(panel);
    gfx::Rect panel_bounds = panel->GetBoundsInRootWindow();
    gfx::Rect callout_bounds = widget->GetWindowBoundsInScreen();
    ASSERT_FALSE(icon_bounds.IsEmpty());

    EXPECT_TRUE(widget->IsVisible());

    ShelfAlignment alignment = GetAlignment();
    switch (alignment) {
      case SHELF_ALIGNMENT_BOTTOM:
        EXPECT_EQ(panel_bounds.bottom(), callout_bounds.y());
        break;
      case SHELF_ALIGNMENT_LEFT:
        EXPECT_EQ(panel_bounds.x(), callout_bounds.right());
        break;
      case SHELF_ALIGNMENT_RIGHT:
        EXPECT_EQ(panel_bounds.right(), callout_bounds.x());
        break;
      case SHELF_ALIGNMENT_TOP:
        EXPECT_EQ(panel_bounds.y(), callout_bounds.bottom());
        break;
    }

    if (IsHorizontal(alignment)) {
      EXPECT_NEAR(icon_bounds.CenterPoint().x(),
                  widget->GetWindowBoundsInScreen().CenterPoint().x(),
                  1);
    } else {
      EXPECT_NEAR(icon_bounds.CenterPoint().y(),
                  widget->GetWindowBoundsInScreen().CenterPoint().y(),
                  1);
    }
  }

  bool IsPanelCalloutVisible(aura::Window* panel) {
    views::Widget* widget = GetCalloutWidgetForPanel(panel);
    return widget->IsVisible();
  }

  test::LauncherViewTestAPI* launcher_view_test() {
    return launcher_view_test_.get();
  }

  // Clicks the launcher items on |launcher_view| that is
  /// associated with given |window|.
  void ClickLauncherItemForWindow(LauncherView* launcher_view,
                                  aura::Window* window) {
    test::LauncherViewTestAPI test_api(launcher_view);
    test_api.SetAnimationDuration(1);
    test_api.RunMessageLoopUntilAnimationsDone();

    LauncherModel* model =
        test::ShellTestApi(Shell::GetInstance()).launcher_model();
    test::TestLauncherDelegate* launcher_delegate =
        test::TestLauncherDelegate::instance();
    int index = model->ItemIndexByID(launcher_delegate->GetIDByWindow(window));
    gfx::Rect bounds = test_api.GetButton(index)->GetBoundsInScreen();

    aura::test::EventGenerator& event_generator = GetEventGenerator();
    event_generator.MoveMouseTo(bounds.CenterPoint());
    event_generator.ClickLeftButton();

    test_api.RunMessageLoopUntilAnimationsDone();
  }

  void SetAlignment(ShelfAlignment alignment) {
    ash::Shell* shell = ash::Shell::GetInstance();
    shell->SetShelfAlignment(alignment, shell->GetPrimaryRootWindow());
  }

  ShelfAlignment GetAlignment() {
    ash::Shell* shell = ash::Shell::GetInstance();
    return shell->GetShelfAlignment(shell->GetPrimaryRootWindow());
  }

 private:
  scoped_ptr<test::LauncherViewTestAPI> launcher_view_test_;

  bool IsHorizontal(ShelfAlignment alignment) {
    return alignment == SHELF_ALIGNMENT_BOTTOM ||
           alignment == SHELF_ALIGNMENT_TOP;
  }

  DISALLOW_COPY_AND_ASSIGN(PanelLayoutManagerTest);
};

// Tests that a created panel window is successfully added to the panel
// layout manager.
TEST_F(PanelLayoutManagerTest, AddOnePanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> window(CreatePanelWindow(bounds));
  EXPECT_EQ(GetPanelContainer(), window->parent());
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(window.get()));
}

// Tests interactions between multiple panels
TEST_F(PanelLayoutManagerTest, MultiplePanelsAreAboveIcons) {
  gfx::Rect odd_bounds(0, 0, 201, 201);
  gfx::Rect even_bounds(0, 0, 200, 200);

  scoped_ptr<aura::Window> w1(CreatePanelWindow(odd_bounds));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));

  scoped_ptr<aura::Window> w2(CreatePanelWindow(even_bounds));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));

  scoped_ptr<aura::Window> w3(CreatePanelWindow(odd_bounds));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w3.get()));
}

TEST_F(PanelLayoutManagerTest, MultiplePanelStacking) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  // Default stacking order.
  EXPECT_TRUE(WindowIsAbove(w3.get(), w2.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w1.get()));

  // Changing the active window should update the stacking order.
  wm::ActivateWindow(w1.get());
  launcher_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(WindowIsAbove(w1.get(), w2.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w3.get()));

  wm::ActivateWindow(w2.get());
  launcher_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(WindowIsAbove(w1.get(), w3.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w3.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w1.get()));

  wm::ActivateWindow(w3.get());
  EXPECT_TRUE(WindowIsAbove(w3.get(), w2.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w1.get()));
}

TEST_F(PanelLayoutManagerTest, MultiplePanelCallout) {
  gfx::Rect bounds(0, 0, 200, 200);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w4(CreateNormalWindow());
  launcher_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(IsPanelCalloutVisible(w1.get()));
  EXPECT_TRUE(IsPanelCalloutVisible(w2.get()));
  EXPECT_TRUE(IsPanelCalloutVisible(w3.get()));
  wm::ActivateWindow(w1.get());
  EXPECT_NO_FATAL_FAILURE(IsCalloutAboveLauncherIcon(w1.get()));
  wm::ActivateWindow(w2.get());
  EXPECT_NO_FATAL_FAILURE(IsCalloutAboveLauncherIcon(w2.get()));
  wm::ActivateWindow(w3.get());
  EXPECT_NO_FATAL_FAILURE(IsCalloutAboveLauncherIcon(w3.get()));
  wm::ActivateWindow(w4.get());
  wm::ActivateWindow(w3.get());
  EXPECT_NO_FATAL_FAILURE(IsCalloutAboveLauncherIcon(w3.get()));
  w3.reset();
  if (views::corewm::UseFocusController())
    EXPECT_NO_FATAL_FAILURE(IsCalloutAboveLauncherIcon(w2.get()));
}

// Tests removing panels.
TEST_F(PanelLayoutManagerTest, RemoveLeftPanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  // At this point, windows should be stacked with 1 < 2 < 3
  wm::ActivateWindow(w1.get());
  launcher_view_test()->RunMessageLoopUntilAnimationsDone();
  // Now, windows should be stacked 1 > 2 > 3
  w1.reset();
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w3.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w3.get()));
}

TEST_F(PanelLayoutManagerTest, RemoveMiddlePanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  // At this point, windows should be stacked with 1 < 2 < 3
  wm::ActivateWindow(w2.get());
  // Windows should be stacked 1 < 2 > 3
  w2.reset();
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w3.get()));
  EXPECT_TRUE(WindowIsAbove(w3.get(), w1.get()));
}

TEST_F(PanelLayoutManagerTest, RemoveRightPanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  // At this point, windows should be stacked with 1 < 2 < 3
  wm::ActivateWindow(w3.get());
  // Order shouldn't change.
  w3.reset();
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w1.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w1.get()));
}

TEST_F(PanelLayoutManagerTest, RemoveNonActivePanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  // At this point, windows should be stacked with 1 < 2 < 3
  wm::ActivateWindow(w2.get());
  // Windows should be stacked 1 < 2 > 3
  w1.reset();
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w2.get()));
  EXPECT_NO_FATAL_FAILURE(IsPanelAboveLauncherIcon(w3.get()));
  EXPECT_TRUE(WindowIsAbove(w2.get(), w3.get()));
}

TEST_F(PanelLayoutManagerTest, SplitView) {
  gfx::Rect bounds(0, 0, 90, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));

  EXPECT_NO_FATAL_FAILURE(PanelsNotOverlapping(w1.get(), w2.get()));
}

#if defined(OS_WIN)
// RootWindow and Display can't resize on Windows Ash. http://crbug.com/165962
#define MAYBE_SplitViewOverlapWhenLarge DISABLED_SplitViewOverlapWhenLarge
#else
#define MAYBE_SplitViewOverlapWhenLarge SplitViewOverlapWhenLarge
#endif

TEST_F(PanelLayoutManagerTest, MAYBE_SplitViewOverlapWhenLarge) {
  gfx::Rect bounds(0, 0, 600, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));

  EXPECT_NO_FATAL_FAILURE(PanelInScreen(w1.get()));
  EXPECT_NO_FATAL_FAILURE(PanelInScreen(w2.get()));
}

TEST_F(PanelLayoutManagerTest, FanWindows) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(bounds));

  launcher_view_test()->RunMessageLoopUntilAnimationsDone();
  int window_x1 = w1->GetBoundsInRootWindow().CenterPoint().x();
  int window_x2 = w2->GetBoundsInRootWindow().CenterPoint().x();
  int window_x3 = w3->GetBoundsInRootWindow().CenterPoint().x();
  Launcher* launcher = Launcher::ForPrimaryDisplay();
  int icon_x1 = launcher->GetScreenBoundsOfItemIconForWindow(w1.get()).x();
  int icon_x2 = launcher->GetScreenBoundsOfItemIconForWindow(w2.get()).x();
  EXPECT_EQ(window_x2 - window_x1, window_x3 - window_x2);
  int spacing = window_x2 - window_x1;
  EXPECT_GT(spacing, icon_x2 - icon_x1);
}

TEST_F(PanelLayoutManagerTest, FanLargeWindow) {
  gfx::Rect small_bounds(0, 0, 201, 201);
  gfx::Rect large_bounds(0, 0, 501, 201);
  scoped_ptr<aura::Window> w1(CreatePanelWindow(small_bounds));
  scoped_ptr<aura::Window> w2(CreatePanelWindow(large_bounds));
  scoped_ptr<aura::Window> w3(CreatePanelWindow(small_bounds));

  launcher_view_test()->RunMessageLoopUntilAnimationsDone();
  int window_x1 = w1->GetBoundsInRootWindow().CenterPoint().x();
  int window_x2 = w2->GetBoundsInRootWindow().CenterPoint().x();
  int window_x3 = w3->GetBoundsInRootWindow().CenterPoint().x();
  // The distances may not be equidistant with a large panel but the panels
  // should be in the correct order with respect to their midpoints.
  EXPECT_GT(window_x2, window_x1);
  EXPECT_GT(window_x3, window_x2);
}

TEST_F(PanelLayoutManagerTest, MinimizeRestorePanel) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> window(CreatePanelWindow(bounds));
  // Activate the window, ensure callout is visible.
  wm::ActivateWindow(window.get());
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsPanelCalloutVisible(window.get()));
  // Minimize the panel, callout should be hidden.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsPanelCalloutVisible(window.get()));
  // Restore the pantel; panel should not be activated by default but callout
  // should be visible.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsPanelCalloutVisible(window.get()));
  // Activate the window, ensure callout is visible.
  wm::ActivateWindow(window.get());
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsPanelCalloutVisible(window.get()));
}

#if defined(OS_WIN)
// Multiple displays aren't supported on Windows Metro/Ash.
// http://crbug.com/165962
#define MAYBE_PanelMoveBetweenMultipleDisplays \
        DISABLED_PanelMoveBetweenMultipleDisplays
#else
#define MAYBE_PanelMoveBetweenMultipleDisplays PanelMoveBetweenMultipleDisplays
#endif

TEST_F(PanelLayoutManagerTest, MAYBE_PanelMoveBetweenMultipleDisplays) {
  // Keep the displays wide so that launchers have enough
  // spaces for launcher buttons.
  UpdateDisplay("600x400,600x400");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();

  scoped_ptr<aura::Window> p1_d1(CreatePanelWindow(gfx::Rect(0, 0, 50, 50)));
  scoped_ptr<aura::Window> p2_d1(CreatePanelWindow(gfx::Rect(0, 0, 50, 50)));
  scoped_ptr<aura::Window> p1_d2(CreatePanelWindow(gfx::Rect(600, 0, 50, 50)));
  scoped_ptr<aura::Window> p2_d2(CreatePanelWindow(gfx::Rect(600, 0, 50, 50)));

  LauncherView* launcher_view_1st =
      Launcher::ForPrimaryDisplay()->GetLauncherViewForTest();
  LauncherView* launcher_view_2nd =
      Launcher::ForWindow(root_windows[1])->GetLauncherViewForTest();

  EXPECT_EQ(root_windows[0], p1_d1->GetRootWindow());
  EXPECT_EQ(root_windows[0], p2_d1->GetRootWindow());
  EXPECT_EQ(root_windows[1], p1_d2->GetRootWindow());
  EXPECT_EQ(root_windows[1], p2_d2->GetRootWindow());

  EXPECT_EQ(internal::kShellWindowId_PanelContainer, p1_d1->parent()->id());
  EXPECT_EQ(internal::kShellWindowId_PanelContainer, p2_d1->parent()->id());
  EXPECT_EQ(internal::kShellWindowId_PanelContainer, p1_d2->parent()->id());
  EXPECT_EQ(internal::kShellWindowId_PanelContainer, p2_d2->parent()->id());

  // Test a panel on 1st display.
  // Clicking on the same display has no effect.
  ClickLauncherItemForWindow(launcher_view_1st, p1_d1.get());
  EXPECT_EQ(root_windows[0], p1_d1->GetRootWindow());
  EXPECT_EQ(root_windows[0], p2_d1->GetRootWindow());
  EXPECT_EQ(root_windows[1], p1_d2->GetRootWindow());
  EXPECT_EQ(root_windows[1], p1_d2->GetRootWindow());
  EXPECT_FALSE(root_windows[1]->GetBoundsInScreen().Contains(
      p1_d1->GetBoundsInScreen()));

  // Test if clicking on another display moves the panel to
  // that display.
  ClickLauncherItemForWindow(launcher_view_2nd, p1_d1.get());
  EXPECT_EQ(root_windows[1], p1_d1->GetRootWindow());
  EXPECT_EQ(root_windows[0], p2_d1->GetRootWindow());
  EXPECT_EQ(root_windows[1], p1_d2->GetRootWindow());
  EXPECT_EQ(root_windows[1], p2_d2->GetRootWindow());
  EXPECT_TRUE(root_windows[1]->GetBoundsInScreen().Contains(
      p1_d1->GetBoundsInScreen()));

  // Test a panel on 2nd display.
  // Clicking on the same display has no effect.
  ClickLauncherItemForWindow(launcher_view_2nd, p1_d2.get());
  EXPECT_EQ(root_windows[1], p1_d1->GetRootWindow());
  EXPECT_EQ(root_windows[0], p2_d1->GetRootWindow());
  EXPECT_EQ(root_windows[1], p1_d2->GetRootWindow());
  EXPECT_EQ(root_windows[1], p2_d2->GetRootWindow());
  EXPECT_TRUE(root_windows[1]->GetBoundsInScreen().Contains(
      p1_d2->GetBoundsInScreen()));

  // Test if clicking on another display moves the panel to
  // that display.
  ClickLauncherItemForWindow(launcher_view_1st, p1_d2.get());
  EXPECT_EQ(root_windows[1], p1_d1->GetRootWindow());
  EXPECT_EQ(root_windows[0], p2_d1->GetRootWindow());
  EXPECT_EQ(root_windows[0], p1_d2->GetRootWindow());
  EXPECT_EQ(root_windows[1], p2_d2->GetRootWindow());
  EXPECT_TRUE(root_windows[0]->GetBoundsInScreen().Contains(
      p1_d2->GetBoundsInScreen()));
}

TEST_F(PanelLayoutManagerTest, AlignmentLeft) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w(CreatePanelWindow(bounds));
  SetAlignment(SHELF_ALIGNMENT_LEFT);
  IsPanelAboveLauncherIcon(w.get());
  IsCalloutAboveLauncherIcon(w.get());
}

TEST_F(PanelLayoutManagerTest, AlignmentRight) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w(CreatePanelWindow(bounds));
  SetAlignment(SHELF_ALIGNMENT_RIGHT);
  IsPanelAboveLauncherIcon(w.get());
  IsCalloutAboveLauncherIcon(w.get());
}

TEST_F(PanelLayoutManagerTest, AlignmentTop) {
  gfx::Rect bounds(0, 0, 201, 201);
  scoped_ptr<aura::Window> w(CreatePanelWindow(bounds));
  SetAlignment(SHELF_ALIGNMENT_TOP);
  IsPanelAboveLauncherIcon(w.get());
  IsCalloutAboveLauncherIcon(w.get());
}

}  // namespace internal
}  // namespace ash
