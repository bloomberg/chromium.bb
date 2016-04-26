// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "ash/accessibility_delegate.h"
#include "ash/ash_switches.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/shelf_test_api.h"
#include "ash/test/shelf_view_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_shelf_delegate.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/common/wm_event.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/user_action_tester.h"
#include "base/thread_task_runner_handle.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/manager/display_layout.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_delegate.h"

namespace ash {
namespace {

const char kActiveWindowChangedFromOverview[] =
    "WindowSelector_ActiveWindowChanged";

class NonActivatableActivationDelegate
    : public aura::client::ActivationDelegate {
 public:
  bool ShouldActivate() const override { return false; }
};

void CancelDrag(DragDropController* controller, bool* canceled) {
  if (controller->IsDragDropInProgress()) {
    *canceled = true;
    controller->DragCancel();
  }
}

}  // namespace

// TODO(bruthig): Move all non-simple method definitions out of class
// declaration.
class WindowSelectorTest : public test::AshTestBase {
 public:
  WindowSelectorTest() {}
  ~WindowSelectorTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnableStableOverviewOrder);
    test::AshTestBase::SetUp();
    ASSERT_TRUE(test::TestShelfDelegate::instance());

    shelf_view_test_.reset(new test::ShelfViewTestAPI(
        test::ShelfTestAPI(Shelf::ForPrimaryDisplay()).shelf_view()));
    shelf_view_test_->SetAnimationDuration(1);
  }

  aura::Window* CreateWindow(const gfx::Rect& bounds) {
    return CreateTestWindowInShellWithDelegate(&delegate_, -1, bounds);
  }

  aura::Window* CreateWindowWithId(const gfx::Rect& bounds, int id) {
    return CreateTestWindowInShellWithDelegate(&delegate_, id, bounds);
  }
  aura::Window* CreateNonActivatableWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateWindow(bounds);
    aura::client::SetActivationDelegate(window,
                                        &non_activatable_activation_delegate_);
    EXPECT_FALSE(ash::wm::CanActivateWindow(window));
    return window;
  }

  // Creates a Widget containing a Window with the given |bounds|. This should
  // be used when the test requires a Widget. For example any test that will
  // cause a window to be closed via
  // views::Widget::GetWidgetForNativeView(window)->Close().
  std::unique_ptr<views::Widget> CreateWindowWidget(const gfx::Rect& bounds) {
    std::unique_ptr<views::Widget> widget(new views::Widget);
    views::Widget::InitParams params;
    params.bounds = bounds;
    params.type = views::Widget::InitParams::TYPE_WINDOW;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget->Init(params);
    widget->Show();
    ParentWindowInPrimaryRootWindow(widget->GetNativeWindow());
    return widget;
  }

  aura::Window* CreatePanelWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        nullptr, ui::wm::WINDOW_TYPE_PANEL, 0, bounds);
    test::TestShelfDelegate::instance()->AddShelfItem(window);
    shelf_view_test()->RunMessageLoopUntilAnimationsDone();
    return window;
  }

  bool WindowsOverlapping(aura::Window* window1, aura::Window* window2) {
    gfx::RectF window1_bounds = GetTransformedTargetBounds(window1);
    gfx::RectF window2_bounds = GetTransformedTargetBounds(window2);
    return window1_bounds.Intersects(window2_bounds);
  }

  void ToggleOverview() {
    ash::Shell::GetInstance()->window_selector_controller()->ToggleOverview();
  }

  gfx::RectF GetTransformedBounds(aura::Window* window) {
    gfx::RectF bounds(ScreenUtil::ConvertRectToScreen(
        window->parent(), window->layer()->bounds()));
    gfx::Transform transform(gfx::TransformAboutPivot(
        gfx::ToFlooredPoint(bounds.origin()),
        window->layer()->transform()));
    transform.TransformRect(&bounds);
    return bounds;
  }

  gfx::RectF GetTransformedTargetBounds(aura::Window* window) {
    gfx::RectF bounds(ScreenUtil::ConvertRectToScreen(
        window->parent(), window->layer()->GetTargetBounds()));
    gfx::Transform transform(gfx::TransformAboutPivot(
        gfx::ToFlooredPoint(bounds.origin()),
        window->layer()->GetTargetTransform()));
    transform.TransformRect(&bounds);
    return bounds;
  }

  gfx::RectF GetTransformedBoundsInRootWindow(aura::Window* window) {
    gfx::RectF bounds = gfx::RectF(gfx::SizeF(window->bounds().size()));
    aura::Window* root = window->GetRootWindow();
    CHECK(window->layer());
    CHECK(root->layer());
    gfx::Transform transform;
    if (!window->layer()->GetTargetTransformRelativeTo(root->layer(),
                                                       &transform)) {
      return gfx::RectF();
    }
    transform.TransformRect(&bounds);
    return bounds;
  }

  void ClickWindow(aura::Window* window) {
    ui::test::EventGenerator event_generator(window->GetRootWindow(), window);
    gfx::RectF target = GetTransformedBounds(window);
    event_generator.ClickLeftButton();
  }

  void SendKey(ui::KeyboardCode key) {
    ui::test::EventGenerator event_generator(Shell::GetPrimaryRootWindow());
      event_generator.PressKey(key, 0);
      event_generator.ReleaseKey(key, 0);
  }

  bool IsSelecting() {
    return ash::Shell::GetInstance()->window_selector_controller()->
        IsSelecting();
  }

  aura::Window* GetFocusedWindow() {
    return aura::client::GetFocusClient(
        Shell::GetPrimaryRootWindow())->GetFocusedWindow();
    }

  const std::vector<WindowSelectorItem*>& GetWindowItemsForRoot(int index) {
    return ash::Shell::GetInstance()->window_selector_controller()->
        window_selector_->grid_list_[index]->window_list_.get();
  }

  WindowSelectorItem* GetWindowItemForWindow(int grid_index,
                                             aura::Window* window) {
    const std::vector<WindowSelectorItem*>& windows =
        GetWindowItemsForRoot(grid_index);
    auto iter = std::find_if(windows.cbegin(), windows.cend(),
                             [window](const WindowSelectorItem* item) {
                               return item->Contains(window);
                             });
    if (iter == windows.end())
      return nullptr;
    return *iter;
  }

  // Selects |window| in the active overview session by cycling through all
  // windows in overview until it is found. Returns true if |window| was found,
  // false otherwise.
  bool SelectWindow(const aura::Window* window) {
    if (GetSelectedWindow() == nullptr)
      SendKey(ui::VKEY_TAB);
    const aura::Window* start_window = GetSelectedWindow();
    if (start_window == window)
      return true;
    do {
      SendKey(ui::VKEY_TAB);
    } while (GetSelectedWindow() != window &&
             GetSelectedWindow() != start_window);
    return GetSelectedWindow() == window;
  }

  const aura::Window* GetSelectedWindow() {
    WindowSelector* ws = ash::Shell::GetInstance()->
        window_selector_controller()->window_selector_.get();
    WindowSelectorItem* item =
        ws->grid_list_[ws->selected_grid_index_]->SelectedWindow();
    if (!item)
      return nullptr;
    return item->GetWindow();
  }

  bool selection_widget_active() {
    WindowSelector* ws = ash::Shell::GetInstance()->
        window_selector_controller()->window_selector_.get();
    return ws->grid_list_[ws->selected_grid_index_]->is_selecting();
  }

  bool showing_filter_widget() {
    WindowSelector* ws = ash::Shell::GetInstance()->
        window_selector_controller()->window_selector_.get();
    return ws->text_filter_widget_->GetNativeWindow()->layer()->
        GetTargetTransform().IsIdentity();
  }

  views::Widget* GetCloseButton(ash::WindowSelectorItem* window) {
    return &(window->close_button_widget_);
  }

  views::LabelButton* GetLabelButtonView(ash::WindowSelectorItem* window) {
    return window->window_label_button_view_;
  }

  // Tests that a window is contained within a given WindowSelectorItem, and
  // that both the window and its matching close button are within the same
  // screen.
  void IsWindowAndCloseButtonInScreen(aura::Window* window,
                                      WindowSelectorItem* window_item) {
    aura::Window* root_window = window_item->root_window();
    EXPECT_TRUE(window_item->Contains(window));
    EXPECT_TRUE(root_window->GetBoundsInScreen().Contains(
        ToEnclosingRect(GetTransformedTargetBounds(window))));
    EXPECT_TRUE(root_window->GetBoundsInScreen().Contains(
        ToEnclosingRect(GetTransformedTargetBounds(
            GetCloseButton(window_item)->GetNativeView()))));
  }

  void FilterItems(const base::StringPiece& pattern) {
    ash::Shell::GetInstance()->
        window_selector_controller()->window_selector_.get()->
            ContentsChanged(nullptr, base::UTF8ToUTF16(pattern));
  }

  test::ShelfViewTestAPI* shelf_view_test() {
    return shelf_view_test_.get();
  }

  views::Widget* text_filter_widget() {
    return ash::Shell::GetInstance()->
        window_selector_controller()->window_selector_.get()->
            text_filter_widget_.get();
  }

 private:
  aura::test::TestWindowDelegate delegate_;
  NonActivatableActivationDelegate non_activatable_activation_delegate_;
  std::unique_ptr<test::ShelfViewTestAPI> shelf_view_test_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorTest);
};

// Tests that the text field in the overview menu is repositioned and resized
// after a screen rotation.
#if defined(OS_WIN) && !defined(USE_ASH)
// TODO(msw): Broken on Windows. http://crbug.com/584038
#define MAYBE_OverviewScreenRotation DISABLED_OverviewScreenRotation
#else
#define MAYBE_OverviewScreenRotation OverviewScreenRotation
#endif
TEST_F(WindowSelectorTest, MAYBE_OverviewScreenRotation) {
  gfx::Rect bounds(0, 0, 400, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> panel1(CreatePanelWindow(bounds));

  // In overview mode the windows should no longer overlap and the text filter
  // widget should be focused.
  ToggleOverview();

  views::Widget* text_filter = text_filter_widget();
  UpdateDisplay("400x300");

  // Formula for initial placement found in window_selector.cc using
  // width = 400, height = 300:
  // x: root_window->bounds().width() / 2 * (1 - kTextFilterScreenProportion).
  // y: -kTextFilterDistanceFromTop (since there's no text in the filter).
  // w: root_window->bounds().width() * kTextFilterScreenProportion.
  // h: kTextFilterHeight.
  EXPECT_EQ("150,-32 100x32",
            text_filter->GetClientAreaBoundsInScreen().ToString());

  // Rotates the display, which triggers the WindowSelector's
  // RepositionTextFilterOnDisplayMetricsChange method.
  UpdateDisplay("400x300/r");

  // Uses the same formulas as abuve using width = 300, height = 400.
  EXPECT_EQ("112,-32 75x32",
            text_filter->GetClientAreaBoundsInScreen().ToString());
}

// Tests that an a11y alert is sent on entering overview mode.
TEST_F(WindowSelectorTest, A11yAlertOnOverviewMode) {
  gfx::Rect bounds(0, 0, 400, 400);
  AccessibilityDelegate* delegate =
      ash::Shell::GetInstance()->accessibility_delegate();
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  EXPECT_NE(delegate->GetLastAccessibilityAlert(),
            ui::A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED);
  ToggleOverview();
  EXPECT_EQ(delegate->GetLastAccessibilityAlert(),
            ui::A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED);
}

// Tests that there are no crashes when there is not enough screen space
// available to show all of the windows.
TEST_F(WindowSelectorTest, SmallDisplay) {
  UpdateDisplay("3x1");
  gfx::Rect bounds(0, 0, 1, 1);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  ToggleOverview();
}

// Tests entering overview mode with two windows and selecting one by clicking.
TEST_F(WindowSelectorTest, Basic) {
  gfx::Rect bounds(0, 0, 400, 400);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> panel1(CreatePanelWindow(bounds));
  std::unique_ptr<aura::Window> panel2(CreatePanelWindow(bounds));

  EXPECT_TRUE(WindowsOverlapping(window1.get(), window2.get()));
  EXPECT_TRUE(WindowsOverlapping(panel1.get(), panel2.get()));
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
  EXPECT_EQ(window2.get(), GetFocusedWindow());
  // Hide the cursor before entering overview to test that it will be shown.
  aura::client::GetCursorClient(root_window)->HideCursor();

  // In overview mode the windows should no longer overlap and the text filter
  // widget should be focused.
  ToggleOverview();
  EXPECT_EQ(text_filter_widget()->GetNativeWindow(), GetFocusedWindow());
  EXPECT_FALSE(WindowsOverlapping(window1.get(), window2.get()));
  EXPECT_FALSE(WindowsOverlapping(window1.get(), panel1.get()));
  EXPECT_FALSE(WindowsOverlapping(panel1.get(), panel2.get()));

  // Clicking window 1 should activate it.
  ClickWindow(window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  EXPECT_EQ(window1.get(), GetFocusedWindow());

  // Cursor should have been unlocked.
  EXPECT_FALSE(aura::client::GetCursorClient(root_window)->IsCursorLocked());
}

// Tests that the ordering of windows is near the windows' original positions.
TEST_F(WindowSelectorTest, MinimizeMovement) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  gfx::Rect left_bounds(0, 0, root_window->bounds().width() / 2,
                        root_window->bounds().height());
  gfx::Rect right_bounds(root_window->bounds().width() / 2, 0,
                         root_window->bounds().width() / 2,
                         root_window->bounds().height());
  std::unique_ptr<aura::Window> left_window(CreateWindow(left_bounds));
  std::unique_ptr<aura::Window> right_window(CreateWindow(right_bounds));

  // The window should stay on the same side of the screen regardless of which
  // one was active on entering overview mode.
  wm::GetWindowState(left_window.get())->Activate();
  ToggleOverview();
  const std::vector<WindowSelectorItem*>& overview1(GetWindowItemsForRoot(0));
  EXPECT_EQ(overview1[0]->GetWindow(), left_window.get());
  EXPECT_EQ(overview1[1]->GetWindow(), right_window.get());
  ToggleOverview();

  // Active the right window, the order should be the same.
  wm::GetWindowState(right_window.get())->Activate();
  ToggleOverview();
  const std::vector<WindowSelectorItem*>& overview2(GetWindowItemsForRoot(0));
  EXPECT_EQ(overview2[0]->GetWindow(), left_window.get());
  EXPECT_EQ(overview2[1]->GetWindow(), right_window.get());
  ToggleOverview();

  // Switch the window positions, and the order should be switched.
  left_window->SetBounds(right_bounds);
  right_window->SetBounds(left_bounds);
  ToggleOverview();
  const std::vector<WindowSelectorItem*>& overview3(GetWindowItemsForRoot(0));
  EXPECT_EQ(overview3[0]->GetWindow(), right_window.get());
  EXPECT_EQ(overview3[1]->GetWindow(), left_window.get());
  ToggleOverview();
}

// Tests that the ordering of windows is near the windows' original positions
// on a second display.
TEST_F(WindowSelectorTest, MinimizeMovementSecondDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  // Verify the same works on the second display
  UpdateDisplay("400x400,400x400");
  gfx::Rect left_bounds(400, 0, 200, 400);
  gfx::Rect right_bounds(600, 0, 200, 400);
  std::unique_ptr<aura::Window> left_window(CreateWindow(left_bounds));
  std::unique_ptr<aura::Window> right_window(CreateWindow(right_bounds));

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(root_windows[1], left_window->GetRootWindow());
  EXPECT_EQ(root_windows[1], right_window->GetRootWindow());

  wm::GetWindowState(left_window.get())->Activate();
  ToggleOverview();
  const std::vector<WindowSelectorItem*>& overview1(GetWindowItemsForRoot(0));
  EXPECT_EQ(overview1[0]->GetWindow(), left_window.get());
  EXPECT_EQ(overview1[1]->GetWindow(), right_window.get());
  ToggleOverview();

  // Active the right window, the order should be the same.
  wm::GetWindowState(right_window.get())->Activate();
  ToggleOverview();
  const std::vector<WindowSelectorItem*>& overview2(GetWindowItemsForRoot(0));
  EXPECT_EQ(overview2[0]->GetWindow(), left_window.get());
  EXPECT_EQ(overview2[1]->GetWindow(), right_window.get());
  ToggleOverview();
}

// Tests that the ordering of windows is stable across different overview
// sessions even when the windows have the same bounds.
TEST_F(WindowSelectorTest, StableOrder) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindowWithId(bounds, 1));
  std::unique_ptr<aura::Window> window2(CreateWindowWithId(bounds, 2));

  // The initial ordering is not defined, but should remain consistent the next
  // time overview is started.
  wm::GetWindowState(window1.get())->Activate();
  ToggleOverview();
  const std::vector<WindowSelectorItem*>& overview1(GetWindowItemsForRoot(0));
  int initial_order[2] = {overview1[0]->GetWindow()->id(),
                          overview1[1]->GetWindow()->id()};
  ToggleOverview();

  // Activate the other window, the order should be the same.
  wm::GetWindowState(window2.get())->Activate();
  ToggleOverview();
  const std::vector<WindowSelectorItem*>& overview2(GetWindowItemsForRoot(0));
  EXPECT_EQ(initial_order[0], overview2[0]->GetWindow()->id());
  EXPECT_EQ(initial_order[1], overview2[1]->GetWindow()->id());
  ToggleOverview();
}

// Tests entering overview mode with docked windows
TEST_F(WindowSelectorTest, BasicWithDocked) {
  // aura::Window* root_window = Shell::GetPrimaryRootWindow();
  gfx::Rect bounds(300, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> docked1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> docked2(CreateWindow(bounds));

  wm::WMEvent dock_event(wm::WM_EVENT_DOCK);
  wm::GetWindowState(docked1.get())->OnWMEvent(&dock_event);

  wm::WindowState* docked_state2 = wm::GetWindowState(docked2.get());
  docked_state2->OnWMEvent(&dock_event);
  wm::WMEvent minimize_event(wm::WM_EVENT_MINIMIZE);
  docked_state2->OnWMEvent(&minimize_event);

  EXPECT_TRUE(WindowsOverlapping(window1.get(), window2.get()));
  gfx::Rect docked_bounds = docked1->GetBoundsInScreen();

  EXPECT_NE(bounds.ToString(), docked_bounds.ToString());
  EXPECT_FALSE(WindowsOverlapping(window1.get(), docked1.get()));
  EXPECT_FALSE(WindowsOverlapping(window1.get(), docked2.get()));
  EXPECT_FALSE(docked2->IsVisible());

  EXPECT_EQ(wm::WINDOW_STATE_TYPE_DOCKED,
            wm::GetWindowState(docked1.get())->GetStateType());
  EXPECT_EQ(wm::WINDOW_STATE_TYPE_DOCKED_MINIMIZED,
            wm::GetWindowState(docked2.get())->GetStateType());

  ToggleOverview();

  EXPECT_FALSE(WindowsOverlapping(window1.get(), window2.get()));
  // Docked windows stays the same.
  EXPECT_EQ(docked_bounds.ToString(), docked1->GetBoundsInScreen().ToString());
  EXPECT_FALSE(docked2->IsVisible());

  // Docked window can still be activated, which will exit the overview mode.
  ClickWindow(docked1.get());
  EXPECT_TRUE(wm::IsActiveWindow(docked1.get()));
  EXPECT_FALSE(
      ash::Shell::GetInstance()->window_selector_controller()->IsSelecting());
}

// Tests selecting a window by tapping on it.
TEST_F(WindowSelectorTest, BasicGesture) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window1.get());
  EXPECT_EQ(window1.get(), GetFocusedWindow());
  ToggleOverview();
  EXPECT_EQ(text_filter_widget()->GetNativeWindow(), GetFocusedWindow());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window2.get());
  generator.GestureTapAt(gfx::ToEnclosingRect(
      GetTransformedTargetBounds(window2.get())).CenterPoint());
  EXPECT_EQ(window2.get(), GetFocusedWindow());
}

// Tests that the user action WindowSelector_ActiveWindowChanged is
// recorded when the mouse/touchscreen/keyboard are used to select a window
// in overview mode which is different from the previously-active window.
TEST_F(WindowSelectorTest, ActiveWindowChangedUserActionRecorded) {
  base::UserActionTester user_action_tester;
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  // Tap on |window2| to activate it and exit overview.
  wm::ActivateWindow(window1.get());
  ToggleOverview();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window2.get());
  generator.GestureTapAt(
      gfx::ToEnclosingRect(GetTransformedTargetBounds(window2.get()))
          .CenterPoint());
  EXPECT_EQ(
      1, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // Click on |window2| to activate it and exit overview.
  wm::ActivateWindow(window1.get());
  ToggleOverview();
  ClickWindow(window2.get());
  EXPECT_EQ(
      2, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // Select |window2| using the arrow keys. Activate it (and exit overview) by
  // pressing the return key.
  wm::ActivateWindow(window1.get());
  ToggleOverview();
  ASSERT_TRUE(SelectWindow(window2.get()));
  SendKey(ui::VKEY_RETURN);
  EXPECT_EQ(
      3, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));
}

// Tests that the user action WindowSelector_ActiveWindowChanged is not
// recorded when the mouse/touchscreen/keyboard are used to select the
// already-active window from overview mode. Also verifies that entering and
// exiting overview without selecting a window does not record the action.
TEST_F(WindowSelectorTest, ActiveWindowChangedUserActionNotRecorded) {
  base::UserActionTester user_action_tester;
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  // Set |window1| to be initially active.
  wm::ActivateWindow(window1.get());
  ToggleOverview();

  // Tap on |window1| to exit overview.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window1.get());
  generator.GestureTapAt(
      gfx::ToEnclosingRect(GetTransformedTargetBounds(window1.get()))
          .CenterPoint());
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // |window1| remains active. Click on it to exit overview.
  ASSERT_EQ(window1.get(), GetFocusedWindow());
  ToggleOverview();
  ClickWindow(window1.get());
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // |window1| remains active. Select using the keyboard.
  ASSERT_EQ(window1.get(), GetFocusedWindow());
  ToggleOverview();
  ASSERT_TRUE(SelectWindow(window1.get()));
  SendKey(ui::VKEY_RETURN);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // Entering and exiting overview without user input should not record
  // the action.
  ToggleOverview();
  ToggleOverview();
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));
}

// Tests that the user action WindowSelector_ActiveWindowChanged is not
// recorded when overview mode exits as a result of closing its only window.
TEST_F(WindowSelectorTest, ActiveWindowChangedUserActionWindowClose) {
  base::UserActionTester user_action_tester;
  std::unique_ptr<views::Widget> widget =
      CreateWindowWidget(gfx::Rect(0, 0, 400, 400));

  ToggleOverview();

  aura::Window* window = widget->GetNativeWindow();
  gfx::RectF bounds = GetTransformedBoundsInRootWindow(window);
  gfx::Point point(bounds.top_right().x() - 1, bounds.top_right().y() - 1);
  ui::test::EventGenerator event_generator(window->GetRootWindow(), point);

  ASSERT_FALSE(widget->IsClosed());
  event_generator.ClickLeftButton();
  ASSERT_TRUE(widget->IsClosed());
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));
}

// Tests that we do not crash and overview mode remains engaged if the desktop
// is tapped while a finger is already down over a window.
TEST_F(WindowSelectorTest, NoCrashWithDesktopTap) {
  std::unique_ptr<aura::Window> window(
      CreateWindow(gfx::Rect(200, 300, 250, 450)));

  ToggleOverview();

  gfx::Rect bounds =
      gfx::ToEnclosingRect(GetTransformedBoundsInRootWindow(window.get()));
  ui::test::EventGenerator event_generator(window->GetRootWindow(),
                                           bounds.CenterPoint());

  // Press down on the window.
  const int kTouchId = 19;
  event_generator.PressTouchId(kTouchId);

  // Tap on the desktop, which should not cause a crash. Overview mode should
  // be disengaged.
  event_generator.GestureTapAt(gfx::Point(0, 0));
  EXPECT_FALSE(IsSelecting());

  event_generator.ReleaseTouchId(kTouchId);
}

// Tests that we do not crash and a window is selected when appropriate when
// we click on a window during touch.
TEST_F(WindowSelectorTest, ClickOnWindowDuringTouch) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  ToggleOverview();

  gfx::Rect window1_bounds =
      gfx::ToEnclosingRect(GetTransformedBoundsInRootWindow(window1.get()));
  ui::test::EventGenerator event_generator(window1->GetRootWindow(),
                                           window1_bounds.CenterPoint());

  // Clicking on |window2| while touching on |window1| should not cause a
  // crash, overview mode should be disengaged and |window2| should be active.
  const int kTouchId = 19;
  event_generator.PressTouchId(kTouchId);
  event_generator.MoveMouseToCenterOf(window2.get());
  event_generator.ClickLeftButton();
  EXPECT_FALSE(IsSelecting());
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
  event_generator.ReleaseTouchId(kTouchId);

  ToggleOverview();

  // Clicking on |window1| while touching on |window1| should not cause
  // a crash, overview mode should be disengaged, and |window1| should
  // be active.
  event_generator.MoveMouseToCenterOf(window1.get());
  event_generator.PressTouchId(kTouchId);
  event_generator.ClickLeftButton();
  EXPECT_FALSE(IsSelecting());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  event_generator.ReleaseTouchId(kTouchId);
}

// Tests that a window does not receive located events when in overview mode.
TEST_F(WindowSelectorTest, WindowDoesNotReceiveEvents) {
  gfx::Rect window_bounds(20, 10, 200, 300);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  std::unique_ptr<aura::Window> window(CreateWindow(window_bounds));

  gfx::Point point1(window_bounds.x() + 10, window_bounds.y() + 10);

  ui::MouseEvent event1(ui::ET_MOUSE_PRESSED, point1, point1,
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);

  ui::EventTarget* root_target = root_window;
  ui::EventTargeter* targeter = root_target->GetEventTargeter();

  // The event should target the window because we are still not in overview
  // mode.
  EXPECT_EQ(window.get(), targeter->FindTargetForEvent(root_target, &event1));

  ToggleOverview();

  // The bounds have changed, take that into account.
  gfx::RectF bounds = GetTransformedBoundsInRootWindow(window.get());
  gfx::Point point2(bounds.x() + 10, bounds.y() + 10);
  ui::MouseEvent event2(ui::ET_MOUSE_PRESSED, point2, point2,
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);

  // Now the transparent window should be intercepting this event.
  EXPECT_NE(window.get(), targeter->FindTargetForEvent(root_target, &event2));
}

// Tests that clicking on the close button effectively closes the window.
TEST_F(WindowSelectorTest, CloseButton) {
  std::unique_ptr<views::Widget> widget =
      CreateWindowWidget(gfx::Rect(0, 0, 400, 400));

  ToggleOverview();

  aura::Window* window = widget->GetNativeWindow();
  gfx::RectF bounds = GetTransformedBoundsInRootWindow(window);
  gfx::Point point(bounds.top_right().x() - 1, bounds.top_right().y() - 1);
  ui::test::EventGenerator event_generator(window->GetRootWindow(), point);

  EXPECT_FALSE(widget->IsClosed());
  event_generator.ClickLeftButton();
  EXPECT_TRUE(widget->IsClosed());
}

// Tests that clicking on the close button on a secondary display effectively
// closes the window.
TEST_F(WindowSelectorTest, CloseButtonOnMultipleDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x400,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  std::unique_ptr<aura::Window> window1(
      CreateWindow(gfx::Rect(650, 300, 250, 450)));

  // We need a widget for the close button to work because windows are closed
  // via the widget. We also use the widget to determine if the window has been
  // closed or not. We explicity create the widget so that the window can be
  // parented to a non-primary root window.
  std::unique_ptr<views::Widget> widget(new views::Widget);
  views::Widget::InitParams params;
  params.bounds = gfx::Rect(650, 0, 400, 400);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = window1->parent();
  widget->Init(params);
  widget->Show();

  ASSERT_EQ(root_windows[1], window1->GetRootWindow());

  ToggleOverview();

  aura::Window* window2 = widget->GetNativeWindow();
  gfx::RectF bounds = GetTransformedBoundsInRootWindow(window2);
  gfx::Point point(bounds.top_right().x() - 1, bounds.top_right().y() - 1);
  ui::test::EventGenerator event_generator(window2->GetRootWindow(), point);

  EXPECT_FALSE(widget->IsClosed());
  event_generator.ClickLeftButton();
  EXPECT_TRUE(widget->IsClosed());
}

// Tests entering overview mode with two windows and selecting one.
TEST_F(WindowSelectorTest, FullscreenWindow) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> panel1(CreatePanelWindow(bounds));
  wm::ActivateWindow(window1.get());

  const wm::WMEvent toggle_fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  wm::GetWindowState(window1.get())->OnWMEvent(&toggle_fullscreen_event);
  // The panel is hidden in fullscreen mode.
  EXPECT_FALSE(panel1->IsVisible());
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsFullscreen());

  // Enter overview and select the fullscreen window.
  ToggleOverview();

  // The panel becomes temporarily visible for the overview.
  EXPECT_TRUE(panel1->IsVisible());
  ClickWindow(window1.get());

  // The window is still fullscreen as it was selected. The panel should again
  // be hidden.
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsFullscreen());
  EXPECT_FALSE(panel1->IsVisible());

  // Entering overview and selecting another window, the previous window remains
  // fullscreen.
  // TODO(flackr): Currently the panel remains hidden, but should become visible
  // again.
  ToggleOverview();
  ClickWindow(window2.get());
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsFullscreen());
}

// Tests that the shelf dimming state is removed while in overview and restored
// on exiting overview.
TEST_F(WindowSelectorTest, OverviewUndimsShelf) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  wm::WindowState* window_state = wm::GetWindowState(window1.get());
  window_state->Maximize();
  ash::ShelfWidget* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  EXPECT_TRUE(shelf->GetDimsShelf());
  ToggleOverview();
  EXPECT_FALSE(shelf->GetDimsShelf());
  ToggleOverview();
  EXPECT_TRUE(shelf->GetDimsShelf());
}

// Tests that entering overview when a fullscreen window is active in maximized
// mode correctly applies the transformations to the window and correctly
// updates the window bounds on exiting overview mode: http://crbug.com/401664.
TEST_F(WindowSelectorTest, FullscreenWindowMaximizeMode) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  gfx::Rect normal_window_bounds(window1->bounds());
  const wm::WMEvent toggle_fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  wm::GetWindowState(window1.get())->OnWMEvent(&toggle_fullscreen_event);
  gfx::Rect fullscreen_window_bounds(window1->bounds());
  EXPECT_NE(normal_window_bounds.ToString(),
            fullscreen_window_bounds.ToString());
  EXPECT_EQ(fullscreen_window_bounds.ToString(),
            window2->GetTargetBounds().ToString());
  ToggleOverview();
  // Window 2 would normally resize to normal window bounds on showing the shelf
  // for overview but this is deferred until overview is exited.
  EXPECT_EQ(fullscreen_window_bounds.ToString(),
            window2->GetTargetBounds().ToString());
  EXPECT_FALSE(WindowsOverlapping(window1.get(), window2.get()));
  ToggleOverview();

  // Since the fullscreen window is still active, window2 will still have the
  // larger bounds.
  EXPECT_EQ(fullscreen_window_bounds.ToString(),
            window2->GetTargetBounds().ToString());

  // Enter overview again and select window 2. Selecting window 2 should show
  // the shelf bringing window2 back to the normal bounds.
  ToggleOverview();
  ClickWindow(window2.get());
  EXPECT_EQ(normal_window_bounds.ToString(),
            window2->GetTargetBounds().ToString());
}

// Tests that beginning window selection hides the app list.
TEST_F(WindowSelectorTest, SelectingHidesAppList) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  Shell::GetInstance()->ShowAppList(nullptr);
  EXPECT_TRUE(Shell::GetInstance()->GetAppListTargetVisibility());
  ToggleOverview();
  EXPECT_FALSE(Shell::GetInstance()->GetAppListTargetVisibility());
  ToggleOverview();
}

// Tests that a minimized window's visibility and layer visibility is correctly
// changed when entering overview and restored when leaving overview mode.
// Crashes after the skia roll in http://crrev.com/274114.
// http://crbug.com/379570
TEST_F(WindowSelectorTest, DISABLED_MinimizedWindowVisibility) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  wm::WindowState* window_state = wm::GetWindowState(window1.get());
  window_state->Minimize();
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window1->layer()->GetTargetVisibility());
  {
    ui::ScopedAnimationDurationScaleMode test_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    ToggleOverview();
    EXPECT_TRUE(window1->IsVisible());
    EXPECT_TRUE(window1->layer()->GetTargetVisibility());
  }
  {
    ui::ScopedAnimationDurationScaleMode test_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    ToggleOverview();
    EXPECT_FALSE(window1->IsVisible());
    EXPECT_FALSE(window1->layer()->GetTargetVisibility());
  }
}

// Tests that a bounds change during overview is corrected for.
TEST_F(WindowSelectorTest, BoundsChangeDuringOverview) {
  std::unique_ptr<aura::Window> window(CreateWindow(gfx::Rect(0, 0, 400, 400)));
  ToggleOverview();
  gfx::Rect overview_bounds =
      ToEnclosingRect(GetTransformedTargetBounds(window.get()));
  window->SetBounds(gfx::Rect(200, 0, 200, 200));
  gfx::Rect new_overview_bounds =
      ToEnclosingRect(GetTransformedTargetBounds(window.get()));
  EXPECT_EQ(overview_bounds.x(), new_overview_bounds.x());
  EXPECT_EQ(overview_bounds.y(), new_overview_bounds.y());
  EXPECT_EQ(overview_bounds.width(), new_overview_bounds.width());
  EXPECT_EQ(overview_bounds.height(), new_overview_bounds.height());
  ToggleOverview();
}

// Tests that a newly created window aborts overview.
TEST_F(WindowSelectorTest, NewWindowCancelsOveriew) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  ToggleOverview();
  EXPECT_TRUE(IsSelecting());

  // A window being created should exit overview mode.
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_FALSE(IsSelecting());
}

// Tests that a window activation exits overview mode.
TEST_F(WindowSelectorTest, ActivationCancelsOveriew) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  window2->Focus();
  ToggleOverview();
  EXPECT_TRUE(IsSelecting());

  // A window being activated should exit overview mode.
  window1->Focus();
  EXPECT_FALSE(IsSelecting());

  // window1 should be focused after exiting even though window2 was focused on
  // entering overview because we exited due to an activation.
  EXPECT_EQ(window1.get(), GetFocusedWindow());
}

// Tests that exiting overview mode without selecting a window restores focus
// to the previously focused window.
TEST_F(WindowSelectorTest, CancelRestoresFocus) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), GetFocusedWindow());

  // In overview mode, the text filter widget should be focused.
  ToggleOverview();
  EXPECT_EQ(text_filter_widget()->GetNativeWindow(), GetFocusedWindow());

  // If canceling overview mode, focus should be restored.
  ToggleOverview();
  EXPECT_EQ(window.get(), GetFocusedWindow());
}

// Tests that overview mode is exited if the last remaining window is destroyed.
TEST_F(WindowSelectorTest, LastWindowDestroyed) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  ToggleOverview();

  window1.reset();
  window2.reset();
  EXPECT_FALSE(IsSelecting());
}

// Tests that entering overview mode restores a window to its original
// target location.
TEST_F(WindowSelectorTest, QuickReentryRestoresInitialTransform) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  gfx::Rect initial_bounds = ToEnclosingRect(
      GetTransformedBounds(window.get()));
  ToggleOverview();
  // Quickly exit and reenter overview mode. The window should still be
  // animating when we reenter. We cannot short circuit animations for this but
  // we also don't have to wait for them to complete.
  {
    ui::ScopedAnimationDurationScaleMode test_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    ToggleOverview();
    ToggleOverview();
  }
  EXPECT_NE(initial_bounds, ToEnclosingRect(
      GetTransformedTargetBounds(window.get())));
  ToggleOverview();
  EXPECT_FALSE(IsSelecting());
  EXPECT_EQ(initial_bounds, ToEnclosingRect(
      GetTransformedTargetBounds(window.get())));
}

// Tests that windows with modal child windows are transformed with the modal
// child even though not activatable themselves.
TEST_F(WindowSelectorTest, ModalChild) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> child1(CreateWindow(bounds));
  child1->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  ::wm::AddTransientChild(window1.get(), child1.get());
  EXPECT_EQ(window1->parent(), child1->parent());
  ToggleOverview();
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(child1->IsVisible());
  EXPECT_EQ(ToEnclosingRect(GetTransformedTargetBounds(child1.get())),
      ToEnclosingRect(GetTransformedTargetBounds(window1.get())));
  ToggleOverview();
}

// Tests that clicking a modal window's parent activates the modal window in
// overview.
TEST_F(WindowSelectorTest, ClickModalWindowParent) {
  std::unique_ptr<aura::Window> window1(
      CreateWindow(gfx::Rect(0, 0, 180, 180)));
  std::unique_ptr<aura::Window> child1(
      CreateWindow(gfx::Rect(200, 0, 180, 180)));
  child1->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  ::wm::AddTransientChild(window1.get(), child1.get());
  EXPECT_FALSE(WindowsOverlapping(window1.get(), child1.get()));
  EXPECT_EQ(window1->parent(), child1->parent());
  ToggleOverview();
  // Given that their relative positions are preserved, the windows should still
  // not overlap.
  EXPECT_FALSE(WindowsOverlapping(window1.get(), child1.get()));
  ClickWindow(window1.get());
  EXPECT_FALSE(IsSelecting());

  // Clicking on window1 should activate child1.
  EXPECT_TRUE(wm::IsActiveWindow(child1.get()));
}

// Tests that windows remain on the display they are currently on in overview
// mode, and that the close buttons are on matching displays.
TEST_F(WindowSelectorTest, MultipleDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x400,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  gfx::Rect bounds1(0, 0, 400, 400);
  gfx::Rect bounds2(650, 0, 400, 400);

  std::unique_ptr<aura::Window> window1(CreateWindow(bounds1));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds1));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds2));
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds2));
  std::unique_ptr<aura::Window> panel1(CreatePanelWindow(bounds1));
  std::unique_ptr<aura::Window> panel2(CreatePanelWindow(bounds1));
  std::unique_ptr<aura::Window> panel3(CreatePanelWindow(bounds2));
  std::unique_ptr<aura::Window> panel4(CreatePanelWindow(bounds2));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[0], window2->GetRootWindow());
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());
  EXPECT_EQ(root_windows[1], window4->GetRootWindow());

  EXPECT_EQ(root_windows[0], panel1->GetRootWindow());
  EXPECT_EQ(root_windows[0], panel2->GetRootWindow());
  EXPECT_EQ(root_windows[1], panel3->GetRootWindow());
  EXPECT_EQ(root_windows[1], panel4->GetRootWindow());

  // In overview mode, each window remains in the same root window.
  ToggleOverview();
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[0], window2->GetRootWindow());
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());
  EXPECT_EQ(root_windows[1], window4->GetRootWindow());
  EXPECT_EQ(root_windows[0], panel1->GetRootWindow());
  EXPECT_EQ(root_windows[0], panel2->GetRootWindow());
  EXPECT_EQ(root_windows[1], panel3->GetRootWindow());
  EXPECT_EQ(root_windows[1], panel4->GetRootWindow());

  // Window indices are based on top-down order. The reverse of our creation.
  IsWindowAndCloseButtonInScreen(window1.get(),
                                 GetWindowItemForWindow(0, window1.get()));
  IsWindowAndCloseButtonInScreen(window2.get(),
                                 GetWindowItemForWindow(0, window2.get()));
  IsWindowAndCloseButtonInScreen(window3.get(),
                                 GetWindowItemForWindow(1, window3.get()));
  IsWindowAndCloseButtonInScreen(window4.get(),
                                 GetWindowItemForWindow(1, window4.get()));

  IsWindowAndCloseButtonInScreen(panel1.get(),
                                 GetWindowItemForWindow(0, panel1.get()));
  IsWindowAndCloseButtonInScreen(panel2.get(),
                                 GetWindowItemForWindow(0, panel2.get()));
  IsWindowAndCloseButtonInScreen(panel3.get(),
                                 GetWindowItemForWindow(1, panel3.get()));
  IsWindowAndCloseButtonInScreen(panel4.get(),
                                 GetWindowItemForWindow(1, panel4.get()));
}

// Tests shutting down during overview.
TEST_F(WindowSelectorTest, Shutdown) {
  gfx::Rect bounds(0, 0, 400, 400);
  // These windows will be deleted when the test exits and the Shell instance
  // is shut down.
  aura::Window* window1(CreateWindow(bounds));
  aura::Window* window2(CreateWindow(bounds));
  aura::Window* window3(CreatePanelWindow(bounds));
  aura::Window* window4(CreatePanelWindow(bounds));

  wm::ActivateWindow(window4);
  wm::ActivateWindow(window3);
  wm::ActivateWindow(window2);
  wm::ActivateWindow(window1);

  ToggleOverview();
}

// Tests removing a display during overview.
TEST_F(WindowSelectorTest, RemoveDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x400,400x400");
  gfx::Rect bounds1(0, 0, 100, 100);
  gfx::Rect bounds2(450, 0, 100, 100);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds1));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds2));
  std::unique_ptr<aura::Window> window3(CreatePanelWindow(bounds1));
  std::unique_ptr<aura::Window> window4(CreatePanelWindow(bounds2));

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());
  EXPECT_EQ(root_windows[0], window3->GetRootWindow());
  EXPECT_EQ(root_windows[1], window4->GetRootWindow());

  wm::ActivateWindow(window4.get());
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  ToggleOverview();
  EXPECT_TRUE(IsSelecting());
  UpdateDisplay("400x400");
  EXPECT_FALSE(IsSelecting());
}

// Tests starting overview during a drag and drop tracking operation.
// TODO(flackr): Fix memory corruption crash when running locally (not failing
// on bots). See http://crbug.com/342528.
TEST_F(WindowSelectorTest, DISABLED_DragDropInProgress) {
  bool drag_canceled_by_test = false;
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  test::ShellTestApi shell_test_api(Shell::GetInstance());
  ash::DragDropController* drag_drop_controller =
      shell_test_api.drag_drop_controller();
  ui::OSExchangeData data;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&WindowSelectorTest::ToggleOverview, base::Unretained(this)));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&CancelDrag, drag_drop_controller, &drag_canceled_by_test));
  data.SetString(base::UTF8ToUTF16("I am being dragged"));
  drag_drop_controller->StartDragAndDrop(data, window->GetRootWindow(),
      window.get(), gfx::Point(5, 5), ui::DragDropTypes::DRAG_MOVE,
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(drag_canceled_by_test);
  ASSERT_TRUE(IsSelecting());
  RunAllPendingInMessageLoop();
}

// Test that a label is created under the window on entering overview mode.
TEST_F(WindowSelectorTest, CreateLabelUnderWindow) {
  std::unique_ptr<aura::Window> window(CreateWindow(gfx::Rect(0, 0, 100, 100)));
  base::string16 window_title = base::UTF8ToUTF16("My window");
  window->SetTitle(window_title);
  ToggleOverview();
  WindowSelectorItem* window_item = GetWindowItemsForRoot(0).back();
  views::LabelButton* label = GetLabelButtonView(window_item);
  // Has the label view been created?
  ASSERT_TRUE(label);

  // Verify the label matches the window title.
  EXPECT_EQ(label->GetText(), window_title);

  // Update the window title and check that the label is updated, too.
  base::string16 updated_title = base::UTF8ToUTF16("Updated title");
  window->SetTitle(updated_title);
  EXPECT_EQ(label->GetText(), updated_title);

  // Labels are located based on target_bounds, not the actual window item
  // bounds.
  gfx::Rect label_bounds = label->GetWidget()->GetNativeWindow()->bounds();
  EXPECT_EQ(label_bounds, window_item->target_bounds());
}

// Tests that overview updates the window positions if the display orientation
// changes.
TEST_F(WindowSelectorTest, DisplayOrientationChanged) {
  if (!SupportsHostWindowResize())
    return;

  aura::Window* root_window = Shell::GetInstance()->GetPrimaryRootWindow();
  UpdateDisplay("600x200");
  EXPECT_EQ("0,0 600x200", root_window->bounds().ToString());
  gfx::Rect window_bounds(0, 0, 150, 150);
  ScopedVector<aura::Window> windows;
  for (int i = 0; i < 3; i++) {
    windows.push_back(CreateWindow(window_bounds));
  }

  ToggleOverview();
  for (ScopedVector<aura::Window>::iterator iter = windows.begin();
       iter != windows.end(); ++iter) {
    EXPECT_TRUE(root_window->bounds().Contains(
        ToEnclosingRect(GetTransformedTargetBounds(*iter))));
  }

  // Rotate the display, windows should be repositioned to be within the screen
  // bounds.
  UpdateDisplay("600x200/r");
  EXPECT_EQ("0,0 200x600", root_window->bounds().ToString());
  for (ScopedVector<aura::Window>::iterator iter = windows.begin();
       iter != windows.end(); ++iter) {
    EXPECT_TRUE(root_window->bounds().Contains(
        ToEnclosingRect(GetTransformedTargetBounds(*iter))));
  }
}

// Tests traversing some windows in overview mode with the tab key.
TEST_F(WindowSelectorTest, BasicTabKeyNavigation) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  ToggleOverview();

  const std::vector<WindowSelectorItem*>& overview_windows =
      GetWindowItemsForRoot(0);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(GetSelectedWindow(), overview_windows[0]->GetWindow());
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(GetSelectedWindow(), overview_windows[1]->GetWindow());
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(GetSelectedWindow(), overview_windows[0]->GetWindow());
}

// Tests traversing some windows in overview mode with the arrow keys in every
// possible direction.
TEST_F(WindowSelectorTest, BasicArrowKeyNavigation) {
  if (!SupportsHostWindowResize())
    return;
  const size_t test_windows = 9;
  UpdateDisplay("800x600");
  ScopedVector<aura::Window> windows;
  for (size_t i = test_windows; i > 0; i--)
    windows.push_back(CreateWindowWithId(gfx::Rect(0, 0, 100, 100), i));

  ui::KeyboardCode arrow_keys[] = {
      ui::VKEY_RIGHT,
      ui::VKEY_DOWN,
      ui::VKEY_LEFT,
      ui::VKEY_UP
  };
  // Expected window layout, assuming that the text filtering feature is
  // enabled by default (i.e., --ash-disable-text-filtering-in-overview-mode
  // is not being used).
  // +-------+  +-------+  +-------+  +-------+
  // |   1   |  |   2   |  |   3   |  |   4   |
  // +-------+  +-------+  +-------+  +-------+
  // +-------+  +-------+  +-------+  +-------+
  // |   5   |  |   6   |  |   7   |  |   8   |
  // +-------+  +-------+  +-------+  +-------+
  // +-------+
  // |   9   |
  // +-------+
  // Index for each window during a full loop plus wrapping around.
  int index_path_for_direction[][test_windows + 1] = {
      {1, 2, 3, 4, 5, 6, 7, 8, 9, 1},  // Right
      {1, 5, 9, 2, 6, 3, 7, 4, 8, 1},  // Down
      {9, 8, 7, 6, 5, 4, 3, 2, 1, 9},  // Left
      {8, 4, 7, 3, 6, 2, 9, 5, 1, 8}   // Up
  };

  for (size_t key_index = 0; key_index < arraysize(arrow_keys); key_index++) {
    ToggleOverview();
    const std::vector<WindowSelectorItem*>& overview_windows =
        GetWindowItemsForRoot(0);
    for (size_t i = 0; i < test_windows + 1; i++) {
      SendKey(arrow_keys[key_index]);
      // TODO(flackr): Add a more readable error message by constructing a
      // string from the window IDs.
      EXPECT_EQ(GetSelectedWindow()->id(),
                overview_windows[index_path_for_direction[key_index][i] - 1]
                    ->GetWindow()
                    ->id());
    }
    ToggleOverview();
  }
}

// Tests basic selection across multiple monitors.
TEST_F(WindowSelectorTest, BasicMultiMonitorArrowKeyNavigation) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x400,400x400");
  gfx::Rect bounds1(0, 0, 100, 100);
  gfx::Rect bounds2(450, 0, 100, 100);
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds2));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds2));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds1));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds1));

  ToggleOverview();

  const std::vector<WindowSelectorItem*>& overview_root1 =
      GetWindowItemsForRoot(0);
  const std::vector<WindowSelectorItem*>& overview_root2 =
      GetWindowItemsForRoot(1);
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), overview_root1[0]->GetWindow());
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), overview_root1[1]->GetWindow());
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), overview_root2[0]->GetWindow());
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), overview_root2[1]->GetWindow());
}

// Tests first monitor when display order doesn't match left to right screen
// positions.
TEST_F(WindowSelectorTest, MultiMonitorReversedOrder) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x400,400x400");
  Shell::GetInstance()->display_manager()->SetLayoutForCurrentDisplays(
      test::CreateDisplayLayout(display::DisplayPlacement::LEFT, 0));
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  gfx::Rect bounds1(-350, 0, 100, 100);
  gfx::Rect bounds2(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds2));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds1));
  EXPECT_EQ(root_windows[1], window1->GetRootWindow());
  EXPECT_EQ(root_windows[0], window2->GetRootWindow());

  ToggleOverview();

  // Coming from the left to right, we should select window1 first being on the
  // display to the left.
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), window1.get());

  ToggleOverview();
  ToggleOverview();

  // Coming from right to left, we should select window2 first being on the
  // display on the right.
  SendKey(ui::VKEY_LEFT);
  EXPECT_EQ(GetSelectedWindow(), window2.get());
}

// Tests three monitors where the grid becomes empty on one of the monitors.
TEST_F(WindowSelectorTest, ThreeMonitor) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x400,400x400,400x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  gfx::Rect bounds1(0, 0, 100, 100);
  gfx::Rect bounds2(400, 0, 100, 100);
  gfx::Rect bounds3(800, 0, 100, 100);
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds3));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds2));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds1));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());
  EXPECT_EQ(root_windows[2], window3->GetRootWindow());

  ToggleOverview();

  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), window3.get());

  // If the last window on a display closes it should select the previous
  // display's window.
  window3.reset();
  EXPECT_EQ(GetSelectedWindow(), window2.get());
  ToggleOverview();

  window3.reset(CreateWindow(bounds3));
  ToggleOverview();
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_RIGHT);

  // If the window on the second display is removed, the selected window should
  // remain window3.
  EXPECT_EQ(GetSelectedWindow(), window3.get());
  window2.reset();
  EXPECT_EQ(GetSelectedWindow(), window3.get());
}

// Tests selecting a window in overview mode with the return key.
TEST_F(WindowSelectorTest, SelectWindowWithReturnKey) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  ToggleOverview();

  // Pressing the return key without a selection widget should not do anything.
  SendKey(ui::VKEY_RETURN);
  EXPECT_TRUE(IsSelecting());

  // Select the first window.
  ASSERT_TRUE(SelectWindow(window1.get()));
  SendKey(ui::VKEY_RETURN);
  ASSERT_FALSE(IsSelecting());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Select the second window.
  ToggleOverview();
  ASSERT_TRUE(SelectWindow(window2.get()));
  SendKey(ui::VKEY_RETURN);
  EXPECT_FALSE(IsSelecting());
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests that overview mode hides the callout widget.
TEST_F(WindowSelectorTest, WindowOverviewHidesCalloutWidgets) {
  std::unique_ptr<aura::Window> panel1(
      CreatePanelWindow(gfx::Rect(0, 0, 100, 100)));
  wm::WmWindow* wm_panel1 = wm::WmWindowAura::Get(panel1.get());
  std::unique_ptr<aura::Window> panel2(
      CreatePanelWindow(gfx::Rect(0, 0, 100, 100)));
  wm::WmWindow* wm_panel2 = wm::WmWindowAura::Get(panel2.get());
  PanelLayoutManager* panel_manager = PanelLayoutManager::Get(wm_panel1);

  // By default, panel callout widgets are visible.
  EXPECT_TRUE(panel_manager->GetCalloutWidgetForPanel(wm_panel1)->IsVisible());
  EXPECT_TRUE(panel_manager->GetCalloutWidgetForPanel(wm_panel2)->IsVisible());

  // Toggling the overview should hide the callout widgets.
  ToggleOverview();
  EXPECT_FALSE(panel_manager->GetCalloutWidgetForPanel(wm_panel1)->IsVisible());
  EXPECT_FALSE(panel_manager->GetCalloutWidgetForPanel(wm_panel2)->IsVisible());

  // Ending the overview should show them again.
  ToggleOverview();
  EXPECT_TRUE(panel_manager->GetCalloutWidgetForPanel(wm_panel1)->IsVisible());
  EXPECT_TRUE(panel_manager->GetCalloutWidgetForPanel(wm_panel2)->IsVisible());
}

// Creates three windows and tests filtering them by title.
TEST_F(WindowSelectorTest, BasicTextFiltering) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window0(CreateWindow(bounds));
  base::string16 window2_title = base::UTF8ToUTF16("Highway to test");
  base::string16 window1_title = base::UTF8ToUTF16("For those about to test");
  base::string16 window0_title = base::UTF8ToUTF16("We salute you");
  window0->SetTitle(window0_title);
  window1->SetTitle(window1_title);
  window2->SetTitle(window2_title);
  ToggleOverview();

  EXPECT_FALSE(selection_widget_active());
  EXPECT_FALSE(showing_filter_widget());
  FilterItems("Test");

  // The selection widget should appear when filtering starts, and should be
  // selecting one of the matching windows above.
  EXPECT_TRUE(selection_widget_active());
  EXPECT_TRUE(showing_filter_widget());
  // window0 does not contain the text "test".
  EXPECT_NE(GetSelectedWindow(), window0.get());

  // Window 0 has no "test" on it so it should be the only dimmed item.
  const int grid_index = 0;
  std::vector<WindowSelectorItem*> items = GetWindowItemsForRoot(0);
  EXPECT_TRUE(GetWindowItemForWindow(grid_index, window0.get())->dimmed());
  EXPECT_FALSE(GetWindowItemForWindow(grid_index, window1.get())->dimmed());
  EXPECT_FALSE(GetWindowItemForWindow(grid_index, window2.get())->dimmed());

  // No items match the search.
  FilterItems("I'm testing 'n testing");
  EXPECT_TRUE(GetWindowItemForWindow(grid_index, window0.get())->dimmed());
  EXPECT_TRUE(GetWindowItemForWindow(grid_index, window1.get())->dimmed());
  EXPECT_TRUE(GetWindowItemForWindow(grid_index, window2.get())->dimmed());

  // All the items should match the empty string. The filter widget should also
  // disappear.
  FilterItems("");
  EXPECT_FALSE(showing_filter_widget());
  EXPECT_FALSE(GetWindowItemForWindow(grid_index, window0.get())->dimmed());
  EXPECT_FALSE(GetWindowItemForWindow(grid_index, window1.get())->dimmed());
  EXPECT_FALSE(GetWindowItemForWindow(grid_index, window2.get())->dimmed());
}

// Tests selecting in the overview with dimmed and undimmed items.
TEST_F(WindowSelectorTest, TextFilteringSelection) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window0(CreateWindow(bounds));
  base::string16 window2_title = base::UTF8ToUTF16("Rock and roll");
  base::string16 window1_title = base::UTF8ToUTF16("Rock and");
  base::string16 window0_title = base::UTF8ToUTF16("Rock");
  window0->SetTitle(window0_title);
  window1->SetTitle(window1_title);
  window2->SetTitle(window2_title);
  ToggleOverview();
  EXPECT_TRUE(SelectWindow(window0.get()));
  EXPECT_TRUE(selection_widget_active());

  // Dim the first item, the selection should jump to the next item.
  std::vector<WindowSelectorItem*> items = GetWindowItemsForRoot(0);
  FilterItems("Rock and");
  EXPECT_NE(GetSelectedWindow(), window0.get());

  // Cycle the selection, the dimmed window should not be selected.
  EXPECT_FALSE(SelectWindow(window0.get()));

  // Dimming all the items should hide the selection widget.
  FilterItems("Pop");
  EXPECT_FALSE(selection_widget_active());

  // Undimming one window should automatically select it.
  FilterItems("Rock and roll");
  EXPECT_EQ(GetSelectedWindow(), window2.get());
}

// Tests clicking on the desktop itself to cancel overview mode.
TEST_F(WindowSelectorTest, CancelOverviewOnMouseClick) {
  // Overview disabled by default.
  EXPECT_FALSE(IsSelecting());

  // Point and bounds selected so that they don't intersect. This causes
  // events located at the point to be passed to DesktopBackgroundController,
  // and not the window.
  gfx::Point point_in_background_page(0, 0);
  gfx::Rect bounds(10, 10, 100, 100);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  ui::test::EventGenerator& generator = GetEventGenerator();
  // Move mouse to point in the background page. Sending an event here will pass
  // it to the DesktopBackgroundController in both regular and overview mode.
  generator.MoveMouseTo(point_in_background_page);

  // Clicking on the background page while not in overview should not toggle
  // overview.
  generator.ClickLeftButton();
  EXPECT_FALSE(IsSelecting());

  // Switch to overview mode.
  ToggleOverview();
  ASSERT_TRUE(IsSelecting());

  // Click should now exit overview mode.
  generator.ClickLeftButton();
  EXPECT_FALSE(IsSelecting());
}

// Tests tapping on the desktop itself to cancel overview mode.
TEST_F(WindowSelectorTest, CancelOverviewOnTap) {
  // Overview disabled by default.
  EXPECT_FALSE(IsSelecting());

  // Point and bounds selected so that they don't intersect. This causes
  // events located at the point to be passed to DesktopBackgroundController,
  // and not the window.
  gfx::Point point_in_background_page(0, 0);
  gfx::Rect bounds(10, 10, 100, 100);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  ui::test::EventGenerator& generator = GetEventGenerator();

  // Tapping on the background page while not in overview should not toggle
  // overview.
  generator.GestureTapAt(point_in_background_page);
  EXPECT_FALSE(IsSelecting());

  // Switch to overview mode.
  ToggleOverview();
  ASSERT_TRUE(IsSelecting());

  // Tap should now exit overview mode.
  generator.GestureTapAt(point_in_background_page);
  EXPECT_FALSE(IsSelecting());
}

}  // namespace ash
