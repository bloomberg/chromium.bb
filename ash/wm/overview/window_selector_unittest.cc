// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/app_list/test_app_list_presenter_impl.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/system/tray/system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_overview_overlay.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/user_action_tester.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_delegate.h"

namespace ash {
namespace {

// The label covers selector item windows with a padding in order to prevent
// them from receiving user input events while in overview.
static const int kWindowMargin = 5;

// The overview mode header overlaps original window header. This value is used
// to set top inset property on the windows.
static const int kHeaderHeight = 32;

const char kActiveWindowChangedFromOverview[] =
    "WindowSelector_ActiveWindowChanged";

// A simple window delegate that returns the specified hit-test code when
// requested and applies a minimum size constraint if there is one.
class TestDragWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  TestDragWindowDelegate() { set_window_component(HTCAPTION); }
  ~TestDragWindowDelegate() override = default;

 private:
  // Overridden from aura::Test::TestWindowDelegate:
  void OnWindowDestroyed(aura::Window* window) override { delete this; }

  DISALLOW_COPY_AND_ASSIGN(TestDragWindowDelegate);
};

class NonActivatableActivationDelegate : public ::wm::ActivationDelegate {
 public:
  bool ShouldActivate() const override { return false; }
};

float GetItemScale(const gfx::Rect& source,
                   const gfx::Rect& target,
                   int top_view_inset,
                   int title_height) {
  return ScopedTransformOverviewWindow::GetItemScale(
      source.size(), target.size(), top_view_inset, title_height);
}

}  // namespace

// TODO(bruthig): Move all non-simple method definitions out of class
// declaration.
class WindowSelectorTest : public AshTestBase {
 public:
  WindowSelectorTest() = default;
  ~WindowSelectorTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    shelf_view_test_.reset(
        new ShelfViewTestAPI(GetPrimaryShelf()->GetShelfViewForTesting()));
    shelf_view_test_->SetAnimationDuration(1);
    ScopedTransformOverviewWindow::SetImmediateCloseForTests();
  }

  aura::Window* CreateWindow(const gfx::Rect& bounds) {
    aura::Window* window =
        CreateTestWindowInShellWithDelegate(&delegate_, -1, bounds);
    window->SetProperty(aura::client::kTopViewInset, kHeaderHeight);
    return window;
  }

  aura::Window* CreateWindowWithId(const gfx::Rect& bounds, int id) {
    aura::Window* window =
        CreateTestWindowInShellWithDelegate(&delegate_, id, bounds);
    window->SetProperty(aura::client::kTopViewInset, kHeaderHeight);
    return window;
  }
  aura::Window* CreateNonActivatableWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateWindow(bounds);
    ::wm::SetActivationDelegate(window, &non_activatable_activation_delegate_);
    EXPECT_FALSE(wm::CanActivateWindow(window));
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
    aura::Window* window = widget->GetNativeWindow();
    window->SetProperty(aura::client::kTopViewInset, kHeaderHeight);
    ParentWindowInPrimaryRootWindow(window);
    return widget;
  }

  aura::Window* CreatePanelWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        nullptr, aura::client::WINDOW_TYPE_PANEL, 0, bounds);
    static int id = 0;
    std::string shelf_id(ShelfID(base::IntToString(id++)).Serialize());
    window->SetProperty(kShelfIDKey, new std::string(shelf_id));
    window->SetProperty<int>(kShelfItemTypeKey, TYPE_APP_PANEL);
    window->SetProperty(aura::client::kTopViewInset, kHeaderHeight);
    shelf_view_test()->RunMessageLoopUntilAnimationsDone();
    return window;
  }

  bool WindowsOverlapping(aura::Window* window1, aura::Window* window2) {
    gfx::Rect window1_bounds = GetTransformedTargetBounds(window1);
    gfx::Rect window2_bounds = GetTransformedTargetBounds(window2);
    return window1_bounds.Intersects(window2_bounds);
  }

  WindowSelectorController* window_selector_controller() {
    return Shell::Get()->window_selector_controller();
  }

  WindowSelector* window_selector() {
    return window_selector_controller()->window_selector_.get();
  }

  void ToggleOverview() { window_selector_controller()->ToggleOverview(); }

  aura::Window* GetOverviewWindowForMinimizedState(int index,
                                                   aura::Window* window) {
    WindowSelectorItem* selector = GetWindowItemForWindow(index, window);
    return selector->GetOverviewWindowForMinimizedStateForTest();
  }

  gfx::Rect GetTransformedBounds(aura::Window* window) {
    gfx::Rect bounds_in_screen = window->layer()->bounds();
    ::wm::ConvertRectToScreen(window->parent(), &bounds_in_screen);
    gfx::RectF bounds(bounds_in_screen);
    gfx::Transform transform(gfx::TransformAboutPivot(
        gfx::ToFlooredPoint(bounds.origin()), window->layer()->transform()));
    transform.TransformRect(&bounds);
    return gfx::ToEnclosingRect(bounds);
  }

  gfx::Rect GetTransformedTargetBounds(aura::Window* window) {
    gfx::Rect bounds_in_screen = window->layer()->GetTargetBounds();
    ::wm::ConvertRectToScreen(window->parent(), &bounds_in_screen);
    gfx::RectF bounds(bounds_in_screen);
    gfx::Transform transform(
        gfx::TransformAboutPivot(gfx::ToFlooredPoint(bounds.origin()),
                                 window->layer()->GetTargetTransform()));
    transform.TransformRect(&bounds);
    return gfx::ToEnclosingRect(bounds);
  }

  gfx::Rect GetTransformedBoundsInRootWindow(aura::Window* window) {
    gfx::RectF bounds = gfx::RectF(gfx::SizeF(window->bounds().size()));
    aura::Window* root = window->GetRootWindow();
    CHECK(window->layer());
    CHECK(root->layer());
    gfx::Transform transform;
    if (!window->layer()->GetTargetTransformRelativeTo(root->layer(),
                                                       &transform)) {
      return gfx::Rect();
    }
    transform.TransformRect(&bounds);
    return gfx::ToEnclosingRect(bounds);
  }

  void ClickWindow(aura::Window* window) {
    ui::test::EventGenerator event_generator(window->GetRootWindow(), window);
    event_generator.ClickLeftButton();
  }

  void SendKey(ui::KeyboardCode key) {
    ui::test::EventGenerator event_generator(Shell::GetPrimaryRootWindow());
    event_generator.PressKey(key, 0);
    event_generator.ReleaseKey(key, 0);
  }

  void SendCtrlKey(ui::KeyboardCode key) {
    ui::test::EventGenerator event_generator(Shell::GetPrimaryRootWindow());
    event_generator.PressKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN);
    event_generator.PressKey(key, ui::EF_CONTROL_DOWN);
    event_generator.ReleaseKey(key, ui::EF_CONTROL_DOWN);
    event_generator.ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  }

  bool IsSelecting() { return window_selector_controller()->IsSelecting(); }

  const std::vector<std::unique_ptr<WindowSelectorItem>>& GetWindowItemsForRoot(
      int index) {
    return window_selector()->grid_list_[index]->window_list();
  }

  WindowSelectorItem* GetWindowItemForWindow(int grid_index,
                                             aura::Window* window) {
    const std::vector<std::unique_ptr<WindowSelectorItem>>& windows =
        GetWindowItemsForRoot(grid_index);
    auto iter =
        std::find_if(windows.cbegin(), windows.cend(),
                     [window](const std::unique_ptr<WindowSelectorItem>& item) {
                       return item->Contains(window);
                     });
    if (iter == windows.end())
      return nullptr;
    return iter->get();
  }

  gfx::SlideAnimation* GetBackgroundViewAnimationForWindow(
      int grid_index,
      aura::Window* window) {
    return GetWindowItemForWindow(grid_index, window)
        ->GetBackgroundViewAnimation();
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
    WindowSelector* ws = window_selector();
    WindowSelectorItem* item =
        ws->grid_list_[ws->selected_grid_index_]->SelectedWindow();
    if (!item)
      return nullptr;
    return item->GetWindow();
  }

  bool selection_widget_active() {
    WindowSelector* ws = window_selector();
    return ws->grid_list_[ws->selected_grid_index_]->is_selecting();
  }

  bool showing_filter_widget() {
    return window_selector()
        ->text_filter_widget_->GetNativeWindow()
        ->layer()
        ->GetTargetTransform()
        .IsIdentity();
  }

  views::Widget* GetCloseButton(WindowSelectorItem* window) {
    return window->close_button_->GetWidget();
  }

  views::Label* GetLabelView(WindowSelectorItem* window) {
    return window->label_view_;
  }

  // Tests that a window is contained within a given WindowSelectorItem, and
  // that both the window and its matching close button are within the same
  // screen.
  void IsWindowAndCloseButtonInScreen(aura::Window* window,
                                      WindowSelectorItem* window_item) {
    aura::Window* root_window = window_item->root_window();
    EXPECT_TRUE(window_item->Contains(window));
    EXPECT_TRUE(root_window->GetBoundsInScreen().Contains(
        GetTransformedTargetBounds(window)));
    EXPECT_TRUE(
        root_window->GetBoundsInScreen().Contains(GetTransformedTargetBounds(
            GetCloseButton(window_item)->GetNativeView())));
  }

  void FilterItems(const base::StringPiece& pattern) {
    window_selector()->ContentsChanged(nullptr, base::UTF8ToUTF16(pattern));
  }

  ShelfViewTestAPI* shelf_view_test() { return shelf_view_test_.get(); }

  views::Widget* text_filter_widget() {
    return window_selector()->text_filter_widget_.get();
  }

  gfx::Rect GetGridBounds() {
    if (window_selector())
      return window_selector()->grid_list_[0]->bounds_;

    return gfx::Rect();
  }

  OverviewWindowDragController* window_drag_controller() {
    DCHECK(window_selector());
    return window_selector()->window_drag_controller_.get();
  }

 private:
  aura::test::TestWindowDelegate delegate_;
  NonActivatableActivationDelegate non_activatable_activation_delegate_;
  std::unique_ptr<ShelfViewTestAPI> shelf_view_test_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorTest);
};

// Tests that the text field in the overview menu is repositioned and resized
// after a screen rotation.
TEST_F(WindowSelectorTest, OverviewScreenRotation) {
  gfx::Rect bounds(0, 0, 400, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> panel1(CreatePanelWindow(bounds));

  // In overview mode the windows should no longer overlap and the text filter
  // widget should be focused.
  ToggleOverview();

  views::Widget* text_filter = text_filter_widget();
  UpdateDisplay("400x300");

  // The text filter position is calculated as:
  // x: 0.5 * (total_bounds.width() -
  //           std::min(kTextFilterWidth, total_bounds.width())).
  // y: -kTextFilterHeight (since there's no text in the filter) - 2.
  // w: std::min(kTextFilterWidth, total_bounds.width()).
  // h: kTextFilterHeight.
  gfx::Rect expected_bounds(60, -42, 280, 40);
  EXPECT_EQ(expected_bounds.ToString(),
            text_filter->GetClientAreaBoundsInScreen().ToString());

  // Rotates the display, which triggers the WindowSelector's
  // RepositionTextFilterOnDisplayMetricsChange method.
  UpdateDisplay("400x300/r");

  // Uses the same formulas as above using width = 300, height = 400.
  expected_bounds = gfx::Rect(10, -42, 280, 40);
  EXPECT_EQ(expected_bounds.ToString(),
            text_filter->GetClientAreaBoundsInScreen().ToString());
}

// Tests that an a11y alert is sent on entering overview mode.
TEST_F(WindowSelectorTest, A11yAlertOnOverviewMode) {
  gfx::Rect bounds(0, 0, 400, 400);
  TestAccessibilityControllerClient client;
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->SetClient(client.CreateInterfacePtrAndBind());
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  EXPECT_NE(mojom::AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED,
            client.last_a11y_alert());
  ToggleOverview();
  controller->FlushMojoForTest();
  EXPECT_EQ(mojom::AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED,
            client.last_a11y_alert());
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
  window1->SetProperty(aura::client::kTopViewInset, 0);
  window2->SetProperty(aura::client::kTopViewInset, 0);
  window3->SetProperty(aura::client::kTopViewInset, 0);
  window4->SetProperty(aura::client::kTopViewInset, 0);
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
  EXPECT_EQ(window2.get(), wm::GetFocusedWindow());
  // TODO: mash doesn't support CursorClient. http://crbug.com/637853.
  if (Shell::GetAshConfig() != Config::MASH) {
    // Hide the cursor before entering overview to test that it will be shown.
    aura::client::GetCursorClient(root_window)->HideCursor();
  }

  // In overview mode the windows should no longer overlap and the text filter
  // widget should be focused.
  ToggleOverview();
  EXPECT_EQ(text_filter_widget()->GetNativeWindow(), wm::GetFocusedWindow());
  EXPECT_FALSE(WindowsOverlapping(window1.get(), window2.get()));
  EXPECT_FALSE(WindowsOverlapping(window1.get(), panel1.get()));
  EXPECT_FALSE(WindowsOverlapping(panel1.get(), panel2.get()));

  // Clicking window 1 should activate it.
  ClickWindow(window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  EXPECT_EQ(window1.get(), wm::GetFocusedWindow());

  // TODO: mash doesn't support CursorClient. http://crbug.com/637853.
  if (Shell::GetAshConfig() != Config::MASH) {
    // Cursor should have been unlocked.
    EXPECT_FALSE(aura::client::GetCursorClient(root_window)->IsCursorLocked());
  }
}

// Tests activating minimized window.
TEST_F(WindowSelectorTest, ActivateMinimized) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  wm::WMEvent minimize_event(wm::WM_EVENT_MINIMIZE);
  window_state->OnWMEvent(&minimize_event);
  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(0.f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(mojom::WindowStateType::MINIMIZED,
            wm::GetWindowState(window.get())->GetStateType());

  ToggleOverview();

  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(0.f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(mojom::WindowStateType::MINIMIZED, window_state->GetStateType());
  aura::Window* window_for_minimized_window =
      GetOverviewWindowForMinimizedState(0, window.get());
  EXPECT_TRUE(window_for_minimized_window);

  const gfx::Point point =
      GetTransformedBoundsInRootWindow(window_for_minimized_window)
          .CenterPoint();
  ui::test::EventGenerator event_generator(window->GetRootWindow(), point);
  event_generator.ClickLeftButton();

  EXPECT_FALSE(IsSelecting());

  EXPECT_TRUE(window->IsVisible());
  EXPECT_EQ(1.f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(mojom::WindowStateType::NORMAL, window_state->GetStateType());
}

// Tests that entering overview mode with an App-list active properly focuses
// and activates the overview text filter window.
TEST_F(WindowSelectorTest, TextFilterActive) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  wm::ActivateWindow(window1.get());

  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  EXPECT_EQ(window1.get(), wm::GetFocusedWindow());

  // Pass an enum to satisfy the function, it is arbitrary and will not affect
  // histograms.
  Shell::Get()->app_list()->ToggleAppList(GetPrimaryDisplay().id(),
                                          app_list::kShelfButton);

  // Activating overview cancels the App-list which normally would activate the
  // previously active |window1|. Overview mode should properly transfer focus
  // and activation to the text filter widget.
  ToggleOverview();
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(wm::GetFocusedWindow()));
  EXPECT_EQ(text_filter_widget()->GetNativeWindow(), wm::GetFocusedWindow());
}

// Tests that the ordering of windows is stable across different overview
// sessions even when the windows have the same bounds.
TEST_F(WindowSelectorTest, WindowsOrder) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindowWithId(bounds, 1));
  std::unique_ptr<aura::Window> window2(CreateWindowWithId(bounds, 2));
  std::unique_ptr<aura::Window> window3(CreateWindowWithId(bounds, 3));

  // The order of windows in overview mode is MRU.
  wm::GetWindowState(window1.get())->Activate();
  ToggleOverview();
  const std::vector<std::unique_ptr<WindowSelectorItem>>& overview1 =
      GetWindowItemsForRoot(0);
  EXPECT_EQ(1, overview1[0]->GetWindow()->id());
  EXPECT_EQ(3, overview1[1]->GetWindow()->id());
  EXPECT_EQ(2, overview1[2]->GetWindow()->id());
  ToggleOverview();

  // Activate the second window.
  wm::GetWindowState(window2.get())->Activate();
  ToggleOverview();
  const std::vector<std::unique_ptr<WindowSelectorItem>>& overview2 =
      GetWindowItemsForRoot(0);

  // The order should be MRU.
  EXPECT_EQ(2, overview2[0]->GetWindow()->id());
  EXPECT_EQ(1, overview2[1]->GetWindow()->id());
  EXPECT_EQ(3, overview2[2]->GetWindow()->id());
  ToggleOverview();
}

// Tests selecting a window by tapping on it.
TEST_F(WindowSelectorTest, BasicGesture) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window1.get());
  EXPECT_EQ(window1.get(), wm::GetFocusedWindow());
  ToggleOverview();
  EXPECT_EQ(text_filter_widget()->GetNativeWindow(), wm::GetFocusedWindow());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window2.get());
  generator.GestureTapAt(
      GetTransformedTargetBounds(window2.get()).CenterPoint());
  EXPECT_EQ(window2.get(), wm::GetFocusedWindow());
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
      GetTransformedTargetBounds(window2.get()).CenterPoint());
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
      GetTransformedTargetBounds(window1.get()).CenterPoint());
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // |window1| remains active. Click on it to exit overview.
  ASSERT_EQ(window1.get(), wm::GetFocusedWindow());
  ToggleOverview();
  ClickWindow(window1.get());
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // |window1| remains active. Select using the keyboard.
  ASSERT_EQ(window1.get(), wm::GetFocusedWindow());
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
  gfx::Rect bounds = GetTransformedBoundsInRootWindow(window);
  gfx::Point point(bounds.top_right().x() - 1, bounds.top_right().y() + 5);
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

  gfx::Rect bounds = GetTransformedBoundsInRootWindow(window.get());
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

  gfx::Rect window1_bounds = GetTransformedBoundsInRootWindow(window1.get());
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
  ui::EventTargeter* targeter =
      root_window->GetHost()->dispatcher()->GetDefaultEventTargeter();

  // The event should target the window because we are still not in overview
  // mode.
  EXPECT_EQ(window.get(), targeter->FindTargetForEvent(root_target, &event1));

  ToggleOverview();

  // The bounds have changed, take that into account.
  gfx::Rect bounds = GetTransformedBoundsInRootWindow(window.get());
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

  std::unique_ptr<views::Widget> minimized_widget =
      CreateWindowWidget(gfx::Rect(400, 0, 400, 400));
  minimized_widget->Minimize();

  ToggleOverview();

  aura::Window* window = widget->GetNativeWindow();
  gfx::Rect bounds = GetTransformedBoundsInRootWindow(window);
  gfx::Point point(bounds.top_right().x() - 1, bounds.top_right().y() + 5);
  ui::test::EventGenerator event_generator(window->GetRootWindow(), point);

  EXPECT_FALSE(widget->IsClosed());
  event_generator.ClickLeftButton();
  EXPECT_TRUE(widget->IsClosed());

  EXPECT_TRUE(IsSelecting());

  aura::Window* window_for_minimized_window =
      GetOverviewWindowForMinimizedState(0,
                                         minimized_widget->GetNativeWindow());
  ASSERT_TRUE(window_for_minimized_window);
  const gfx::Rect rect =
      GetTransformedBoundsInRootWindow(window_for_minimized_window);

  event_generator.MoveMouseTo(
      gfx::Point(rect.top_right().x() - 10, rect.top_right().y() - 10));

  EXPECT_FALSE(minimized_widget->IsClosed());
  event_generator.ClickLeftButton();
  EXPECT_TRUE(minimized_widget->IsClosed());

  // All minimized windows are closed, so it should exit overview mode.
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsSelecting());
}

// Tests minimizing/unminimizing in overview mode.
TEST_F(WindowSelectorTest, MinimizeUnminimize) {
  std::unique_ptr<views::Widget> widget =
      CreateWindowWidget(gfx::Rect(0, 0, 400, 400));
  aura::Window* window = widget->GetNativeWindow();

  ToggleOverview();

  EXPECT_FALSE(GetOverviewWindowForMinimizedState(0, window));
  widget->Minimize();
  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_TRUE(IsSelecting());

  EXPECT_TRUE(GetOverviewWindowForMinimizedState(0, window));

  widget->Restore();
  EXPECT_FALSE(widget->IsMinimized());

  EXPECT_FALSE(GetOverviewWindowForMinimizedState(0, window));
  EXPECT_TRUE(IsSelecting());
}

// Tests that clicking on the close button on a secondary display effectively
// closes the window.
TEST_F(WindowSelectorTest, CloseButtonOnMultipleDisplay) {
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
  aura::Window* window = widget->GetNativeWindow();
  window->SetProperty(aura::client::kTopViewInset, kHeaderHeight);

  ASSERT_EQ(root_windows[1], window1->GetRootWindow());

  ToggleOverview();

  aura::Window* window2 = widget->GetNativeWindow();
  gfx::Rect bounds = GetTransformedBoundsInRootWindow(window2);
  gfx::Point point(bounds.top_right().x() - 1, bounds.top_right().y() + 5);
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

TEST_F(WindowSelectorTest, SkipOverviewWindow) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  window2->SetProperty(ash::kShowInOverviewKey, false);

  // Enter overview.
  ToggleOverview();
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());

  // Exit overview.
  ToggleOverview();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
}

// Tests that entering overview when a fullscreen window is active in maximized
// mode correctly applies the transformations to the window and correctly
// updates the window bounds on exiting overview mode: http://crbug.com/401664.
TEST_F(WindowSelectorTest, FullscreenWindowTabletMode) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
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

  // The tested behavior relies on the app list presenter delegate.
  TestAppListPresenterImpl app_list_presenter_impl;

  app_list_presenter_impl.ShowAndRunLoop(GetPrimaryDisplay().id());
  EXPECT_TRUE(app_list_presenter_impl.IsVisible());

  ToggleOverview();
  EXPECT_FALSE(app_list_presenter_impl.IsVisible());
  ToggleOverview();
}

// Tests that a minimized window's visibility and layer visibility
// stay invisible (A minimized window is cloned during overview),
// and ignored_by_shelf state is restored upon exit.
TEST_F(WindowSelectorTest, MinimizedWindowState) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  wm::WindowState* window_state = wm::GetWindowState(window1.get());
  window_state->Minimize();
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window1->layer()->GetTargetVisibility());
  EXPECT_FALSE(window_state->ignored_by_shelf());

  ToggleOverview();
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window1->layer()->GetTargetVisibility());
  EXPECT_TRUE(window_state->ignored_by_shelf());

  ToggleOverview();
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window1->layer()->GetTargetVisibility());
  EXPECT_FALSE(window_state->ignored_by_shelf());
}

// Tests that it is safe to destroy a window while the overview header animation
// is still active. See http://crbug.com/646350.
TEST_F(WindowSelectorTest, SafeToDestroyWindowDuringAnimation) {
  gfx::Rect bounds(0, 0, 400, 400);
  {
    // Quickly enter and exit overview mode to activate header animations.
    std::unique_ptr<aura::Window> window(CreateWindow(bounds));
    ui::ScopedAnimationDurationScaleMode test_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    ToggleOverview();
    EXPECT_TRUE(IsSelecting());

    gfx::SlideAnimation* animation =
        GetBackgroundViewAnimationForWindow(0, window.get());
    ASSERT_NE(nullptr, animation);
    ToggleOverview();
    EXPECT_FALSE(IsSelecting());
    if (animation)
      EXPECT_TRUE(animation->is_animating());

    // Close the window while the overview header animation is active.
    window.reset();

    // Progress animation to the end - should not crash.
    if (animation) {
      animation->SetCurrentValue(1.0);
      animation->Reset(1.0);
    }
  }
}

// Tests that a bounds change during overview is corrected for.
TEST_F(WindowSelectorTest, BoundsChangeDuringOverview) {
  std::unique_ptr<aura::Window> window(CreateWindow(gfx::Rect(0, 0, 400, 400)));
  // Use overview headers above the window in this test.
  window->SetProperty(aura::client::kTopViewInset, 0);
  ToggleOverview();
  gfx::Rect overview_bounds = GetTransformedTargetBounds(window.get());
  window->SetBounds(gfx::Rect(200, 0, 200, 200));
  gfx::Rect new_overview_bounds = GetTransformedTargetBounds(window.get());
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
  EXPECT_EQ(window1.get(), wm::GetFocusedWindow());
}

// Tests that exiting overview mode without selecting a window restores focus
// to the previously focused window.
TEST_F(WindowSelectorTest, CancelRestoresFocus) {
  gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), wm::GetFocusedWindow());

  // In overview mode, the text filter widget should be focused.
  ToggleOverview();
  EXPECT_EQ(text_filter_widget()->GetNativeWindow(), wm::GetFocusedWindow());

  // If canceling overview mode, focus should be restored.
  ToggleOverview();
  EXPECT_EQ(window.get(), wm::GetFocusedWindow());
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
  gfx::Rect initial_bounds = GetTransformedBounds(window.get());
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
  EXPECT_NE(initial_bounds, GetTransformedTargetBounds(window.get()));
  ToggleOverview();
  EXPECT_FALSE(IsSelecting());
  EXPECT_EQ(initial_bounds, GetTransformedTargetBounds(window.get()));
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
  EXPECT_EQ(GetTransformedTargetBounds(child1.get()),
            GetTransformedTargetBounds(window1.get()));
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
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreatePanelWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreatePanelWindow(bounds));

  wm::ActivateWindow(window4.get());
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  ToggleOverview();
}

// Tests removing a display during overview.
TEST_F(WindowSelectorTest, RemoveDisplay) {
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

// Tests removing a display during overview with NON_ZERO_DURATION animation.
TEST_F(WindowSelectorTest, RemoveDisplayWithAnimation) {
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

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  UpdateDisplay("400x400");
  EXPECT_FALSE(IsSelecting());
}

// Tests that toggling overview on and off doesnot cancel drag.
TEST_F(WindowSelectorTest, DragDropInProgress) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      new TestDragWindowDelegate(), -1, bounds));

  ui::test::EventGenerator event_generator(window->GetRootWindow(),
                                           window.get());
  event_generator.PressLeftButton();
  event_generator.MoveMouseBy(10, 10);
  EXPECT_EQ(window->bounds().ToString(), "10,10 100x100");

  ToggleOverview();
  ASSERT_TRUE(IsSelecting());

  event_generator.MoveMouseBy(10, 10);

  ToggleOverview();
  ASSERT_FALSE(IsSelecting());

  event_generator.MoveMouseBy(10, 10);
  event_generator.ReleaseLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_EQ(window->bounds().ToString(), "30,30 100x100");

  return;
}

// Test that a label is created under the window on entering overview mode.
TEST_F(WindowSelectorTest, CreateLabelUnderWindow) {
  std::unique_ptr<aura::Window> window(CreateWindow(gfx::Rect(0, 0, 300, 500)));
  base::string16 window_title = base::UTF8ToUTF16("My window");
  window->SetTitle(window_title);
  ToggleOverview();
  WindowSelectorItem* window_item = GetWindowItemsForRoot(0).back().get();
  views::Label* label = GetLabelView(window_item);
  // Has the label view been created?
  ASSERT_TRUE(label);

  // Verify the label matches the window title.
  EXPECT_EQ(label->text(), window_title);

  // Update the window title and check that the label is updated, too.
  base::string16 updated_title = base::UTF8ToUTF16("Updated title");
  window->SetTitle(updated_title);
  EXPECT_EQ(label->text(), updated_title);

  // Labels are located based on target_bounds, not the actual window item
  // bounds.
  gfx::Rect label_bounds = label->GetWidget()->GetWindowBoundsInScreen();
  label_bounds.Inset(kWindowMargin, kWindowMargin);
  EXPECT_EQ(window_item->target_bounds(), label_bounds);
}

// Tests that overview updates the window positions if the display orientation
// changes.
TEST_F(WindowSelectorTest, DisplayOrientationChanged) {
  aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
  UpdateDisplay("600x200");
  EXPECT_EQ("0,0 600x200", root_window->bounds().ToString());
  gfx::Rect window_bounds(0, 0, 150, 150);
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (int i = 0; i < 3; i++)
    windows.push_back(base::WrapUnique(CreateWindow(window_bounds)));

  ToggleOverview();
  for (const auto& window : windows) {
    EXPECT_TRUE(root_window->bounds().Contains(
        GetTransformedTargetBounds(window.get())));
  }

  // Rotate the display, windows should be repositioned to be within the screen
  // bounds.
  UpdateDisplay("600x200/r");
  EXPECT_EQ("0,0 200x600", root_window->bounds().ToString());
  for (const auto& window : windows) {
    EXPECT_TRUE(root_window->bounds().Contains(
        GetTransformedTargetBounds(window.get())));
  }
}

// Tests traversing some windows in overview mode with the tab key.
TEST_F(WindowSelectorTest, BasicTabKeyNavigation) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  ToggleOverview();

  const std::vector<std::unique_ptr<WindowSelectorItem>>& overview_windows =
      GetWindowItemsForRoot(0);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(GetSelectedWindow(), overview_windows[0]->GetWindow());
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(GetSelectedWindow(), overview_windows[1]->GetWindow());
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(GetSelectedWindow(), overview_windows[0]->GetWindow());
}

// Tests that pressing Ctrl+W while a window is selected in overview closes it.
TEST_F(WindowSelectorTest, CloseWindowWithKey) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<views::Widget> widget =
      CreateWindowWidget(gfx::Rect(0, 0, 400, 400));
  aura::Window* window1 = widget->GetNativeWindow();
  ToggleOverview();

  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(window1, GetSelectedWindow());
  SendCtrlKey(ui::VKEY_W);
  EXPECT_TRUE(widget->IsClosed());
}

// Tests traversing some windows in overview mode with the arrow keys in every
// possible direction.
TEST_F(WindowSelectorTest, BasicArrowKeyNavigation) {
  const size_t test_windows = 9;
  UpdateDisplay("800x600");
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (size_t i = test_windows; i > 0; i--) {
    windows.push_back(
        base::WrapUnique(CreateWindowWithId(gfx::Rect(0, 0, 100, 100), i)));
  }

  ui::KeyboardCode arrow_keys[] = {ui::VKEY_RIGHT, ui::VKEY_DOWN, ui::VKEY_LEFT,
                                   ui::VKEY_UP};
  // The rows contain variable number of items making vertical navigation not
  // feasible. [Down] is equivalent to [Right] and [Up] is equivalent to [Left].
  int index_path_for_direction[][test_windows + 1] = {
      {1, 2, 3, 4, 5, 6, 7, 8, 9, 1},  // Right
      {1, 2, 3, 4, 5, 6, 7, 8, 9, 1},  // Down (same as Right)
      {9, 8, 7, 6, 5, 4, 3, 2, 1, 9},  // Left
      {9, 8, 7, 6, 5, 4, 3, 2, 1, 9}   // Up (same as Left)
  };

  for (size_t key_index = 0; key_index < arraysize(arrow_keys); key_index++) {
    ToggleOverview();
    const std::vector<std::unique_ptr<WindowSelectorItem>>& overview_windows =
        GetWindowItemsForRoot(0);
    for (size_t i = 0; i < test_windows + 1; i++) {
      SendKey(arrow_keys[key_index]);
      // TODO(flackr): Add a more readable error message by constructing a
      // string from the window IDs.
      const int index = index_path_for_direction[key_index][i];
      EXPECT_EQ(GetSelectedWindow()->id(),
                overview_windows[index - 1]->GetWindow()->id());
    }
    ToggleOverview();
  }
}

// Verifies hitting the escape and back keys exit overview mode.
TEST_F(WindowSelectorTest, ExitOverviewWithKey) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());
  SendKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(window_selector_controller()->IsSelecting());

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());
  SendKey(ui::VKEY_BROWSER_BACK);
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
}

// Tests basic selection across multiple monitors.
TEST_F(WindowSelectorTest, BasicMultiMonitorArrowKeyNavigation) {
  UpdateDisplay("400x400,400x400");
  gfx::Rect bounds1(0, 0, 100, 100);
  gfx::Rect bounds2(450, 0, 100, 100);
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds2));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds2));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds1));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds1));

  ToggleOverview();

  const std::vector<std::unique_ptr<WindowSelectorItem>>& overview_root1 =
      GetWindowItemsForRoot(0);
  const std::vector<std::unique_ptr<WindowSelectorItem>>& overview_root2 =
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
  UpdateDisplay("400x400,400x400");
  Shell::Get()->display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::LEFT, 0));
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
  std::unique_ptr<aura::Window> panel2(
      CreatePanelWindow(gfx::Rect(0, 0, 100, 100)));
  PanelLayoutManager* panel_manager = PanelLayoutManager::Get(panel1.get());

  // By default, panel callout widgets are visible.
  EXPECT_TRUE(
      panel_manager->GetCalloutWidgetForPanel(panel1.get())->IsVisible());
  EXPECT_TRUE(
      panel_manager->GetCalloutWidgetForPanel(panel2.get())->IsVisible());

  // Toggling the overview should hide the callout widgets.
  ToggleOverview();
  EXPECT_FALSE(
      panel_manager->GetCalloutWidgetForPanel(panel1.get())->IsVisible());
  EXPECT_FALSE(
      panel_manager->GetCalloutWidgetForPanel(panel2.get())->IsVisible());

  // Ending the overview should show them again.
  ToggleOverview();
  EXPECT_TRUE(
      panel_manager->GetCalloutWidgetForPanel(panel1.get())->IsVisible());
  EXPECT_TRUE(
      panel_manager->GetCalloutWidgetForPanel(panel2.get())->IsVisible());
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

  FilterItems("Foo");

  EXPECT_NE(1.0f, window0->layer()->GetTargetOpacity());
  EXPECT_NE(1.0f, window1->layer()->GetTargetOpacity());
  EXPECT_NE(1.0f, window2->layer()->GetTargetOpacity());

  ToggleOverview();

  EXPECT_EQ(1.0f, window0->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window1->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window2->layer()->GetTargetOpacity());
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

// Tests that transferring focus from the text filter to a window that is not a
// top level window does not cancel overview mode.
TEST_F(WindowSelectorTest, ShowTextFilterMenu) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window0(CreateWindow(bounds));
  base::string16 window0_title = base::UTF8ToUTF16("Test");
  window0->SetTitle(window0_title);
  wm::GetWindowState(window0.get())->Minimize();
  ToggleOverview();

  EXPECT_FALSE(selection_widget_active());
  EXPECT_FALSE(showing_filter_widget());
  FilterItems("Test");

  EXPECT_TRUE(selection_widget_active());
  EXPECT_TRUE(showing_filter_widget());

  // Open system bubble shifting focus from the text filter.
  SystemTray* tray = GetPrimarySystemTray();
  tray->ShowDefaultView(BUBBLE_CREATE_NEW, false /* show_by_click */);
  RunAllPendingInMessageLoop();

  // This should not cancel overview mode.
  ASSERT_TRUE(IsSelecting());
  EXPECT_TRUE(selection_widget_active());
  EXPECT_TRUE(showing_filter_widget());

  // Click text filter to bring focus back.
  ui::test::EventGenerator& generator = GetEventGenerator();
  gfx::Point point_in_text_filter =
      text_filter_widget()->GetWindowBoundsInScreen().CenterPoint();
  generator.MoveMouseTo(point_in_text_filter);
  generator.ClickLeftButton();
  EXPECT_TRUE(IsSelecting());

  // Cancel overview mode.
  ToggleOverview();
  ASSERT_FALSE(IsSelecting());
}

// Tests clicking on the desktop itself to cancel overview mode.
TEST_F(WindowSelectorTest, CancelOverviewOnMouseClick) {
  // Overview disabled by default.
  EXPECT_FALSE(IsSelecting());

  // Point and bounds selected so that they don't intersect. This causes
  // events located at the point to be passed to WallpaperController,
  // and not the window.
  gfx::Point point_in_background_page(0, 0);
  gfx::Rect bounds(10, 10, 100, 100);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  ui::test::EventGenerator& generator = GetEventGenerator();
  // Move mouse to point in the background page. Sending an event here will pass
  // it to the WallpaperController in both regular and overview mode.
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
  // events located at the point to be passed to WallpaperController,
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

// Tests that transformed Rect scaling preserves its aspect ratio.
// The window scale is determined by the target height and so the test is
// actually testing that the width is calculated correctly. Since all
// calculations are done with floating point values and then safely converted to
// integers (using ceiled and floored values where appropriate), the
// expectations are forgiving (use *_NEAR) within a single pixel.
TEST_F(WindowSelectorTest, TransformedRectMaintainsAspect) {
  gfx::Rect rect(50, 50, 200, 400);
  gfx::Rect bounds(100, 100, 50, 50);
  gfx::Rect transformed_rect =
      ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
          rect, bounds, 0, 0);
  float scale = GetItemScale(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::Rect(50, 50, 400, 200);
  scale = GetItemScale(rect, bounds, 0, 0);
  transformed_rect =
      ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
          rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::Rect(50, 50, 25, 25);
  scale = GetItemScale(rect, bounds, 0, 0);
  transformed_rect =
      ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
          rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::Rect(50, 50, 25, 50);
  scale = GetItemScale(rect, bounds, 0, 0);
  transformed_rect =
      ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
          rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::Rect(50, 50, 50, 25);
  scale = GetItemScale(rect, bounds, 0, 0);
  transformed_rect =
      ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
          rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);
}

// Tests that transformed Rect fits in target bounds and is vertically centered.
TEST_F(WindowSelectorTest, TransformedRectIsCentered) {
  gfx::Rect rect(50, 50, 200, 400);
  gfx::Rect bounds(100, 100, 50, 50);
  gfx::Rect transformed_rect =
      ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
          rect, bounds, 0, 0);
  EXPECT_GE(transformed_rect.x(), bounds.x());
  EXPECT_LE(transformed_rect.right(), bounds.right());
  EXPECT_GE(transformed_rect.y(), bounds.y());
  EXPECT_LE(transformed_rect.bottom(), bounds.bottom());
  EXPECT_NEAR(transformed_rect.x() - bounds.x(),
              bounds.right() - transformed_rect.right(), 1);
  EXPECT_NEAR(transformed_rect.y() - bounds.y(),
              bounds.bottom() - transformed_rect.bottom(), 1);
}

// Tests that transformed Rect fits in target bounds and is vertically centered
// when inset and header height are specified.
TEST_F(WindowSelectorTest, TransformedRectIsCenteredWithInset) {
  gfx::Rect rect(50, 50, 400, 200);
  gfx::Rect bounds(100, 100, 50, 50);
  const int inset = 20;
  const int header_height = 10;
  const float scale = GetItemScale(rect, bounds, inset, header_height);
  gfx::Rect transformed_rect =
      ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
          rect, bounds, inset, header_height);
  // The |rect| width does not fit and therefore it gets centered outside
  // |bounds| starting before |bounds.x()| and ending after |bounds.right()|.
  EXPECT_LE(transformed_rect.x(), bounds.x());
  EXPECT_GE(transformed_rect.right(), bounds.right());
  EXPECT_GE(
      transformed_rect.y() + gfx::ToCeiledInt(scale * inset) - header_height,
      bounds.y());
  EXPECT_LE(transformed_rect.bottom(), bounds.bottom());
  EXPECT_NEAR(transformed_rect.x() - bounds.x(),
              bounds.right() - transformed_rect.right(), 1);
  EXPECT_NEAR(
      transformed_rect.y() + (int)(scale * inset) - header_height - bounds.y(),
      bounds.bottom() - transformed_rect.bottom(), 1);
}

// Start dragging a window and activate overview mode. This test should not
// crash or DCHECK inside aura::Window::StackChildRelativeTo().
TEST_F(WindowSelectorTest, OverviewWhileDragging) {
  const gfx::Rect bounds(10, 10, 100, 100);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  std::unique_ptr<WindowResizer> resizer(CreateWindowResizer(
      window.get(), gfx::Point(), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_MOUSE));
  ASSERT_TRUE(resizer.get());
  gfx::Point location = resizer->GetInitialLocation();
  location.Offset(20, 20);
  resizer->Drag(location, 0);
  ToggleOverview();
  resizer->RevertDrag();
}

class SplitViewWindowSelectorTest : public WindowSelectorTest {
 public:
  SplitViewWindowSelectorTest() = default;
  ~SplitViewWindowSelectorTest() override = default;

  enum class SelectorItemLocation {
    CENTER,
    ORIGIN,
    TOP_RIGHT,
    BOTTOM_RIGHT,
    BOTTOM_LEFT,
  };

  void SetUp() override {
    WindowSelectorTest::SetUp();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnableTabletSplitView);
    Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  }

  SplitViewController* split_view_controller() {
    return Shell::Get()->split_view_controller();
  }

  void EndSplitView() { split_view_controller()->EndSplitView(); }

 protected:
  aura::Window* CreateWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegate(
        new SplitViewTestWindowDelegate, -1, bounds);
    return window;
  }

  aura::Window* CreateWindowWithMinimumSize(const gfx::Rect& bounds,
                                            const gfx::Size& size) {
    SplitViewTestWindowDelegate* delegate = new SplitViewTestWindowDelegate();
    aura::Window* window =
        CreateTestWindowInShellWithDelegate(delegate, -1, bounds);
    delegate->set_minimum_size(size);
    return window;
  }

  gfx::Rect GetSplitViewLeftWindowBounds(aura::Window* window) {
    return split_view_controller()->GetSnappedWindowBoundsInScreen(
        window, SplitViewController::LEFT);
  }

  gfx::Rect GetSplitViewRightWindowBounds(aura::Window* window) {
    return split_view_controller()->GetSnappedWindowBoundsInScreen(
        window, SplitViewController::RIGHT);
  }

  gfx::Rect GetSplitViewDividerBounds(bool is_dragging) {
    if (!split_view_controller()->IsSplitViewModeActive())
      return gfx::Rect();
    return split_view_controller()
        ->split_view_divider_->GetDividerBoundsInScreen(is_dragging);
  }

  // Drags a window selector item |item| from its center or one of its corners
  // to |end_location|. This should be used over
  // DragWindowTo(WindowSelectorItem*, gfx::Point) when testing snapping a
  // window, but the windows centerpoint may be inside a snap region, thus the
  // window will not snapped.
  void DragWindowTo(WindowSelectorItem* item,
                    const gfx::Point& end_location,
                    SelectorItemLocation location) {
    // Start drag in the middle of the seletor item.
    gfx::Point start_location;
    switch (location) {
      case SelectorItemLocation::CENTER:
        start_location = item->target_bounds().CenterPoint();
        break;
      case SelectorItemLocation::ORIGIN:
        start_location = item->target_bounds().origin();
        break;
      case SelectorItemLocation::TOP_RIGHT:
        start_location = item->target_bounds().top_right();
        break;
      case SelectorItemLocation::BOTTOM_RIGHT:
        start_location = item->target_bounds().bottom_right();
        break;
      case SelectorItemLocation::BOTTOM_LEFT:
        start_location = item->target_bounds().bottom_left();
        break;
      default:
        NOTREACHED();
        break;
    }
    window_selector()->InitiateDrag(item, start_location);
    window_selector()->Drag(item, end_location);
    window_selector()->CompleteDrag(item, end_location);
  }

  // Drags a window selector item |item| from its center point to
  // |end_location|.
  void DragWindowTo(WindowSelectorItem* item, const gfx::Point& end_location) {
    DragWindowTo(item, end_location, SelectorItemLocation::CENTER);
  }

  // Creates a window which cannot be snapped by splitview.
  std::unique_ptr<aura::Window> CreateUnsnappableWindow(
      const gfx::Rect& bounds = gfx::Rect()) {
    std::unique_ptr<aura::Window> window;
    if (bounds.IsEmpty())
      window = CreateTestWindow();
    else
      window = base::WrapUnique<aura::Window>(CreateWindow(bounds));

    window->SetProperty(aura::client::kResizeBehaviorKey,
                        ui::mojom::kResizeBehaviorNone);
    return window;
  }

  IndicatorType indicator_type() {
    DCHECK(window_selector());
    return window_selector()
        ->split_view_overview_overlay()
        ->current_indicator_type();
  }

 private:
  class SplitViewTestWindowDelegate : public aura::test::TestWindowDelegate {
   public:
    SplitViewTestWindowDelegate() = default;
    ~SplitViewTestWindowDelegate() override = default;

    // aura::test::TestWindowDelegate:
    void OnWindowDestroying(aura::Window* window) override { window->Hide(); }
    void OnWindowDestroyed(aura::Window* window) override { delete this; }
  };

  DISALLOW_COPY_AND_ASSIGN(SplitViewWindowSelectorTest);
};

// Tests that dragging a overview window selector item to the edge of the screen
// snaps the window. If two windows are snapped to left and right side of the
// screen, exit the overview mode.
TEST_F(SplitViewWindowSelectorTest, DragOverviewWindowToSnap) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();
  EXPECT_TRUE(window_selector_controller()->IsSelecting());
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Drag |window1| selector item to snap to left.
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));

  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());

  // Drag |window2| selector item to snap to left.
  WindowSelectorItem* selector_item2 =
      GetWindowItemForWindow(grid_index, window2.get());
  DragWindowTo(selector_item2, gfx::Point(0, 0));

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());

  // Drag |window3| selector item to snap to right.
  WindowSelectorItem* selector_item3 =
      GetWindowItemForWindow(grid_index, window3.get());
  const gfx::Rect work_area_rect =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  const gfx::Point end_location3(work_area_rect.width(), 0);
  DragWindowTo(selector_item3, end_location3);

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->right_window(), window3.get());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
}

TEST_F(SplitViewWindowSelectorTest, Dragging) {
  ui::test::EventGenerator& generator = GetEventGenerator();

  std::unique_ptr<aura::Window> right_window = CreateTestWindow();
  std::unique_ptr<aura::Window> left_window = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  WindowSelectorItem* left_selector_item =
      GetWindowItemForWindow(0, left_window.get());
  WindowSelectorItem* right_selector_item =
      GetWindowItemForWindow(0, right_window.get());

  // The inset on each side of the screen which is a snap region. Items dragged
  // to and released under this region will get snapped.
  const int drag_offset = OverviewWindowDragController::kMinimumDragOffset;
  const int drag_offset_snap_region =
      OverviewWindowDragController::kMinimumDragOffsetAlreadyInSnapRegionDp;
  const int edge_inset = OverviewWindowDragController::kScreenEdgeInsetForDrag;
  const int screen_width =
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(left_window.get()).width();
  // The selector item has a margin which does not accept events. Inset any
  // event aimed at the selector items edge so events will reach it.
  const int selector_item_inset = 20;

  // Check the two windows set up have a region which is under no snap region, a
  // region that is under the left snap region and a region that is under the
  // right snap region.
  ASSERT_GT(left_selector_item->target_bounds().CenterPoint().x(), edge_inset);
  ASSERT_LT(
      left_selector_item->target_bounds().origin().x() + selector_item_inset,
      edge_inset);
  ASSERT_GT(right_selector_item->target_bounds().top_right().x() -
                selector_item_inset,
            screen_width - edge_inset);

  // Verify if the drag is not started in either snap region, the drag still
  // must move by |drag_offset| before split view acknowledges the drag (ie.
  // starts moving the selector item).
  generator.set_current_location(
      left_selector_item->target_bounds().CenterPoint());
  generator.PressLeftButton();
  const gfx::Rect left_original_bounds = left_selector_item->target_bounds();
  generator.MoveMouseBy(drag_offset - 1, 0);
  EXPECT_EQ(left_original_bounds, left_selector_item->target_bounds());
  generator.MoveMouseBy(1, 0);
  EXPECT_NE(left_original_bounds, left_selector_item->target_bounds());
  generator.ReleaseLeftButton();

  // Verify if the drag is started in the left snap region, the drag needs to
  // move by |drag_offset_snap_region| towards the left side of the screen
  // before split view acknowledges the drag (shows the phantom window).
  ASSERT_TRUE(window_selector_controller()->IsSelecting());
  generator.set_current_location(gfx::Point(
      left_selector_item->target_bounds().origin().x() + selector_item_inset,
      left_selector_item->target_bounds().CenterPoint().y()));
  generator.PressLeftButton();
  generator.MoveMouseBy(-drag_offset_snap_region + 1, 0);
  EXPECT_FALSE(window_drag_controller()->IsPhantomWindowShowing());
  generator.MoveMouseBy(-1, 0);
  EXPECT_TRUE(window_drag_controller()->IsPhantomWindowShowing());
  // Drag back to the middle before releasing so that we stay in overview mode
  // on release.
  generator.MoveMouseTo(left_original_bounds.CenterPoint());
  generator.ReleaseLeftButton();

  // Verify if the drag is started in the right snap region, the drag needs to
  // move by |drag_offset_snap_region| towards the right side of the screen
  // before split view acknowledges the drag.
  ASSERT_TRUE(window_selector_controller()->IsSelecting());
  generator.set_current_location(
      gfx::Point(right_selector_item->target_bounds().top_right().x() -
                     selector_item_inset,
                 right_selector_item->target_bounds().CenterPoint().y()));
  generator.PressLeftButton();
  generator.MoveMouseBy(drag_offset_snap_region - 1, 0);
  EXPECT_FALSE(window_drag_controller()->IsPhantomWindowShowing());
  generator.MoveMouseBy(1, 0);
  EXPECT_TRUE(window_drag_controller()->IsPhantomWindowShowing());
}

// Verify the window grid size changes as expected when dragging items around in
// overview mode when split view is enabled.
TEST_F(SplitViewWindowSelectorTest, WindowGridSizeWhileDraggingWithSplitView) {
  // Add three windows and enter overview mode.
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  // Select window one and start the drag.
  const int grid_index = 0;
  const int window_width =
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().width();
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(grid_index, window1.get());
  gfx::Rect selector_item_bounds = selector_item->target_bounds();
  gfx::Point start_location(selector_item_bounds.CenterPoint());
  window_selector()->InitiateDrag(selector_item, start_location);

  // Verify that when dragged to the left, the window grid is located where the
  // right window of split view mode should be.
  const gfx::Point left(0, 0);
  window_selector()->Drag(selector_item, left);
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(SplitViewController::NO_SNAP, split_view_controller()->state());
  EXPECT_TRUE(split_view_controller()->left_window() == nullptr);
  EXPECT_EQ(GetSplitViewRightWindowBounds(window1.get()), GetGridBounds());

  // Verify that when dragged to the right, the window grid is located where the
  // left window of split view mode should be.
  const gfx::Point right(window_width, 0);
  window_selector()->Drag(selector_item, right);
  EXPECT_EQ(SplitViewController::NO_SNAP, split_view_controller()->state());
  EXPECT_TRUE(split_view_controller()->right_window() == nullptr);
  EXPECT_EQ(GetSplitViewLeftWindowBounds(window1.get()), GetGridBounds());

  // Verify that when dragged to the center, the window grid is has the
  // dimensions of the work area.
  const gfx::Point center(window_width / 2, 0);
  window_selector()->Drag(selector_item, center);
  EXPECT_EQ(SplitViewController::NO_SNAP, split_view_controller()->state());
  EXPECT_EQ(
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get()),
      GetGridBounds());

  // Snap window1 to the left and initialize dragging for window2.
  window_selector()->Drag(selector_item, left);
  window_selector()->CompleteDrag(selector_item, left);
  ASSERT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());
  ASSERT_EQ(window1.get(), split_view_controller()->left_window());
  selector_item = GetWindowItemForWindow(grid_index, window2.get());
  selector_item_bounds = selector_item->target_bounds();
  start_location = selector_item_bounds.CenterPoint();
  window_selector()->InitiateDrag(selector_item, start_location);

  // Verify that when there is a snapped window, the window grid bounds remain
  // constant despite window selector items being dragged left and right.
  window_selector()->Drag(selector_item, left);
  EXPECT_EQ(GetSplitViewRightWindowBounds(window1.get()), GetGridBounds());
  window_selector()->Drag(selector_item, right);
  EXPECT_EQ(GetSplitViewRightWindowBounds(window1.get()), GetGridBounds());
  window_selector()->Drag(selector_item, center);
  EXPECT_EQ(GetSplitViewRightWindowBounds(window1.get()), GetGridBounds());
}

// Tests dragging a unsnappable window.
TEST_F(SplitViewWindowSelectorTest, DraggingUnsnappableAppWithSplitView) {
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();

  // The grid bounds should be the size of the root window minus the shelf.
  const gfx::Rect root_window_bounds =
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen();
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())->GetIdealBounds();
  const gfx::Rect expected_grid_bounds =
      SubtractRects(root_window_bounds, shelf_bounds);

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  // Verify that after dragging the unsnappable window to the left and right,
  // the window grid bounds do not change.
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(0, unsnappable_window.get());
  window_selector()->InitiateDrag(selector_item,
                                  selector_item->target_bounds().CenterPoint());
  window_selector()->Drag(selector_item, gfx::Point(0, 0));
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
  window_selector()->Drag(selector_item,
                          gfx::Point(root_window_bounds.right(), 0));
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
  window_selector()->Drag(selector_item,
                          gfx::Point(root_window_bounds.right() / 2, 0));
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
}

// Tests that if there is only one window in the MRU window list in the overview
// mode, snapping the window to one side of the screen will end the overview
// mode since there is no more window left in the overview window grid.
TEST_F(SplitViewWindowSelectorTest, EmptyWindowsListExitOverview) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  ToggleOverview();
  EXPECT_TRUE(window_selector_controller()->IsSelecting());

  // Drag |window1| selector item to snap to left.
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));

  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
}

// Verify the split view phantom window becomes visible when visible.
TEST_F(SplitViewWindowSelectorTest, PhantomWindowVisibility) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  const int edge_inset = OverviewWindowDragController::kScreenEdgeInsetForDrag;
  const int screen_width =
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(window.get()).width();

  // Verify the phantom window is visible when |selector_item|'s x is in the
  // range [0, edge_inset] or [screen_width - edge_inset - 1, screen_width].
  const int grid_index = 0;
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(grid_index, window.get());
  const gfx::Point start_location(selector_item->target_bounds().CenterPoint());
  window_selector()->InitiateDrag(selector_item, start_location);
  EXPECT_FALSE(window_drag_controller()->IsPhantomWindowShowing());
  window_selector()->Drag(selector_item, gfx::Point(edge_inset + 1, 1));
  EXPECT_FALSE(window_drag_controller()->IsPhantomWindowShowing());
  window_selector()->Drag(selector_item, gfx::Point(edge_inset, 1));
  EXPECT_TRUE(window_drag_controller()->IsPhantomWindowShowing());

  window_selector()->Drag(selector_item,
                          gfx::Point(screen_width - edge_inset - 2, 1));
  EXPECT_FALSE(window_drag_controller()->IsPhantomWindowShowing());
  window_selector()->Drag(selector_item,
                          gfx::Point(screen_width - edge_inset - 1, 1));
  EXPECT_TRUE(window_drag_controller()->IsPhantomWindowShowing());

  // Drag back to |start_location| before compeleting the drag, otherwise
  // |selector_time| will snap to the right and the system will enter splitview,
  // making |window_drag_controller()| nullptr.
  window_selector()->Drag(selector_item, start_location);
  window_selector()->CompleteDrag(selector_item, start_location);
  EXPECT_FALSE(window_drag_controller()->IsPhantomWindowShowing());
}

// Verify that the phantom window never shows up when dragging a unsnappable
// window.
TEST_F(SplitViewWindowSelectorTest, PhantomWindowVisibilityUnsnappableWindow) {
  std::unique_ptr<aura::Window> window = CreateUnsnappableWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  const int screen_width =
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(window.get()).width();

  const int grid_index = 0;
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(grid_index, window.get());
  const gfx::Point start_location(selector_item->target_bounds().CenterPoint());
  window_selector()->InitiateDrag(selector_item, start_location);
  EXPECT_FALSE(window_drag_controller()->IsPhantomWindowShowing());
  window_selector()->Drag(selector_item, gfx::Point(0, 1));
  EXPECT_FALSE(window_drag_controller()->IsPhantomWindowShowing());
  window_selector()->Drag(selector_item, gfx::Point(screen_width, 1));
  EXPECT_FALSE(window_drag_controller()->IsPhantomWindowShowing());

  window_selector()->CompleteDrag(selector_item, start_location);
  EXPECT_FALSE(window_drag_controller()->IsPhantomWindowShowing());
}

// Verify that the split view overview overlay is shown when expected.
TEST_F(SplitViewWindowSelectorTest, SplitViewOverviewOverlayVisibility) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  const int edge_inset = OverviewWindowDragController::kScreenEdgeInsetForDrag;

  // Verify that when are no snapped windows, the overlay is visible when a drag
  // is initiated and disappears when the drag reaches |edge_inset| from the
  // edge of the screen.
  const int grid_index = 0;
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(grid_index, window1.get());
  gfx::Point start_location(selector_item->target_bounds().CenterPoint());
  window_selector()->InitiateDrag(selector_item, start_location);
  EXPECT_EQ(IndicatorType::DRAG_AREA, indicator_type());
  window_selector()->Drag(selector_item, gfx::Point(edge_inset + 1, 0));
  EXPECT_EQ(IndicatorType::DRAG_AREA, indicator_type());
  window_selector()->Drag(selector_item, gfx::Point(edge_inset, 0));
  EXPECT_EQ(IndicatorType::NONE, indicator_type());

  // Snap window to the left.
  window_selector()->CompleteDrag(selector_item, gfx::Point(edge_inset, 0));
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());
  ASSERT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());

  // Verify that when there is a snapped window, the overlay is not visible.
  selector_item = GetWindowItemForWindow(grid_index, window2.get());
  start_location = selector_item->target_bounds().CenterPoint();
  window_selector()->InitiateDrag(selector_item, start_location);
  EXPECT_EQ(IndicatorType::NONE, indicator_type());
  window_selector()->CompleteDrag(selector_item, start_location);
}

// Verify that the split view overview overlay is shown when expected when
// attempting to drag a unsnappable window.
TEST_F(SplitViewWindowSelectorTest,
       SplitViewOverviewOverlayVisibilityUnsnappableWindow) {
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  const int grid_index = 0;
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(grid_index, unsnappable_window.get());
  gfx::Point start_location(selector_item->target_bounds().CenterPoint());
  window_selector()->InitiateDrag(selector_item, start_location);
  EXPECT_EQ(IndicatorType::CANNOT_SNAP, indicator_type());
  const gfx::Point end_location1(0, 0);
  window_selector()->Drag(selector_item, end_location1);
  EXPECT_EQ(IndicatorType::CANNOT_SNAP, indicator_type());
  window_selector()->CompleteDrag(selector_item, end_location1);
  EXPECT_EQ(IndicatorType::NONE, indicator_type());
}

// Verify that the split view overview overlays widget reparents when starting a
// drag on a different display.
TEST_F(SplitViewWindowSelectorTest, SplitViewOverviewOverlayWidgetReparenting) {
  // Add two displays and one window on each display.
  UpdateDisplay("600x600,600x600");
  auto root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  const gfx::Rect primary_screen_bounds(0, 0, 600, 600);
  const gfx::Rect secondary_screen_bounds(600, 0, 600, 600);
  std::unique_ptr<aura::Window> primary_screen_window(
      CreateWindow(primary_screen_bounds));
  std::unique_ptr<aura::Window> secondary_screen_window(
      CreateWindow(secondary_screen_bounds));

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  // Select an item on the primary display and verify the overlay's widget's
  // parent is the primary root window.
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(0, primary_screen_window.get());
  gfx::Point start_location(selector_item->target_bounds().CenterPoint());
  window_selector()->InitiateDrag(selector_item, start_location);
  EXPECT_EQ(IndicatorType::DRAG_AREA, indicator_type());
  EXPECT_EQ(root_windows[0], window_selector()
                                 ->split_view_overview_overlay()
                                 ->widget_->GetNativeView()
                                 ->GetRootWindow());
  // Drag the item in a way that neither opens the window nor activates
  // splitview mode.
  window_selector()->Drag(selector_item, primary_screen_bounds.CenterPoint());
  window_selector()->CompleteDrag(selector_item,
                                  primary_screen_bounds.CenterPoint());
  ASSERT_TRUE(window_selector());
  ASSERT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Select an item on the secondary display and verify the overlay's widget has
  // reparented to the secondary root window.
  selector_item = GetWindowItemForWindow(1, secondary_screen_window.get());
  start_location = gfx::Point(selector_item->target_bounds().CenterPoint());
  window_selector()->InitiateDrag(selector_item, start_location);
  EXPECT_EQ(IndicatorType::DRAG_AREA, indicator_type());
  EXPECT_EQ(root_windows[1], window_selector()
                                 ->split_view_overview_overlay()
                                 ->widget_->GetNativeView()
                                 ->GetRootWindow());
  window_selector()->CompleteDrag(selector_item, start_location);
}

// Test the overview window drag functionalities when screen rotates.
TEST_F(SplitViewWindowSelectorTest, SplitViewRotationTest) {
  UpdateDisplay("807x407");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  // Set the screen orientation to LANDSCAPE_PRIMARY.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::ROTATION_SOURCE_ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            blink::kWebScreenOrientationLockLandscapePrimary);

  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  // Test that dragging |window1| to the left of the screen snaps it to left.
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());

  // Test that dragging |window2| to the right of the screen snaps it to right.
  WindowSelectorItem* selector_item2 =
      GetWindowItemForWindow(grid_index, window2.get());
  gfx::Rect work_area_rect =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  gfx::Point end_location2(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(selector_item2, end_location2);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EndSplitView();

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::ROTATION_SOURCE_ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            blink::kWebScreenOrientationLockPortraitPrimary);
  ToggleOverview();

  // Test that dragging |window1| to the top of the screen snaps it to right.
  selector_item1 = GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::RIGHT_SNAPPED);
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());

  // Test that dragging |window2| to the bottom of the screen snaps it to left.
  selector_item2 = GetWindowItemForWindow(grid_index, window2.get());
  work_area_rect =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  end_location2 = gfx::Point(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(selector_item2, end_location2, SelectorItemLocation::ORIGIN);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EndSplitView();

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::ROTATION_SOURCE_ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            blink::kWebScreenOrientationLockLandscapeSecondary);
  ToggleOverview();

  // Test that dragging |window1| to the left of the screen snaps it to right.
  selector_item1 = GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::RIGHT_SNAPPED);
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());

  // Test that dragging |window2| to the right of the screen snaps it to left.
  selector_item2 = GetWindowItemForWindow(grid_index, window2.get());
  work_area_rect =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  end_location2 = gfx::Point(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(selector_item2, end_location2, SelectorItemLocation::ORIGIN);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EndSplitView();

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::ROTATION_SOURCE_ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            blink::kWebScreenOrientationLockPortraitSecondary);
  ToggleOverview();

  // Test that dragging |window1| to the top of the screen snaps it to left.
  selector_item1 = GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());

  // Test that dragging |window2| to the bottom of the screen snaps it to right.
  selector_item2 = GetWindowItemForWindow(grid_index, window2.get());
  work_area_rect =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  end_location2 = gfx::Point(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(selector_item2, end_location2);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EndSplitView();
}

// Test that when split view mode and overview mode are both active at the same
// time, dragging the split view divider resizes the bounds of snapped window
// and the bounds of overview window grids at the same time.
TEST_F(SplitViewWindowSelectorTest, SplitViewOverviewBothActiveTest) {
  UpdateDisplay("907x407");

  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();

  // Drag |window1| selector item to snap to left.
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  const gfx::Rect window1_bounds = window1->GetBoundsInScreen();
  const gfx::Rect overview_grid_bounds = GetGridBounds();
  const gfx::Rect divider_bounds =
      GetSplitViewDividerBounds(false /* is_dragging */);

  // Test that window1, divider, overview grid are aligned horizontally.
  EXPECT_EQ(window1_bounds.right(), divider_bounds.x());
  EXPECT_EQ(divider_bounds.right(), overview_grid_bounds.x());

  const gfx::Point resize_start_location(divider_bounds.CenterPoint());
  split_view_controller()->StartResize(resize_start_location);
  const gfx::Point resize_end_location(300, 0);
  split_view_controller()->EndResize(resize_end_location);

  const gfx::Rect window1_bounds_after_resize = window1->GetBoundsInScreen();
  const gfx::Rect overview_grid_bounds_after_resize = GetGridBounds();
  const gfx::Rect divider_bounds_after_resize =
      GetSplitViewDividerBounds(false /* is_dragging */);

  // Test that window1, divider, overview grid are still aligned horizontally
  // after resizing.
  EXPECT_EQ(window1_bounds.right(), divider_bounds.x());
  EXPECT_EQ(divider_bounds.right(), overview_grid_bounds.x());

  // Test that window1, divider, overview grid's bounds are changed after
  // resizing.
  EXPECT_NE(window1_bounds, window1_bounds_after_resize);
  EXPECT_NE(overview_grid_bounds, overview_grid_bounds_after_resize);
  EXPECT_NE(divider_bounds, divider_bounds_after_resize);
}

// Verify that selecting an unsnappable window while in split view works as
// intended.
TEST_F(SplitViewWindowSelectorTest, SelectUnsnappableWindowInSplitView) {
  // Create one snappable and one unsnappable window.
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  // Snap the snappable window to enter split view mode.
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  // Select the unsnappable window.
  const int grid_index = 0;
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(grid_index, unsnappable_window.get());
  GetEventGenerator().set_current_location(
      selector_item->target_bounds().CenterPoint());
  GetEventGenerator().ClickLeftButton();

  // Verify that we are out of split view and overview mode, and that the active
  // window is the unsnappable window.
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
  EXPECT_EQ(unsnappable_window.get(), wm::GetActiveWindow());

  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  // Split view mode should be active. Overview mode should be ended.
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(SplitViewController::BOTH_SNAPPED,
            split_view_controller()->state());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());

  ToggleOverview();
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());
  EXPECT_TRUE(window_selector_controller()->IsSelecting());

  // Now select the unsnappable window.
  selector_item = GetWindowItemForWindow(grid_index, unsnappable_window.get());
  GetEventGenerator().set_current_location(
      selector_item->target_bounds().CenterPoint());
  GetEventGenerator().ClickLeftButton();

  // Split view mode should be ended. And the unsnappable window should be the
  // active window now.
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
  EXPECT_EQ(unsnappable_window.get(), wm::GetActiveWindow());
}

// Verify that when in overview mode, the selector items unsnappable indicator
// shows up when expected.
TEST_F(SplitViewWindowSelectorTest, OverviewUnsnappableIndicatorVisibility) {
  // Create three windows; two normal and one unsnappable, so that when after
  // snapping |window1| to enter split view we can test the state of each normal
  // and unsnappable windows.
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  const int grid_index = 0;
  WindowSelectorItem* snappable_selector_item =
      GetWindowItemForWindow(grid_index, window2.get());
  WindowSelectorItem* unsnappable_selector_item =
      GetWindowItemForWindow(grid_index, unsnappable_window.get());

  // Note: Using IsDrawn() instead of visible() because
  // |cannot_snap_label_view_|'s parent (which handles the padding and rounded
  // corners) is actually the item whose visibility gets altered.
  EXPECT_FALSE(snappable_selector_item->cannot_snap_label_view_->IsDrawn());
  EXPECT_FALSE(unsnappable_selector_item->cannot_snap_label_view_->IsDrawn());

  // Snap the extra snappable window to enter split view mode.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  EXPECT_FALSE(snappable_selector_item->cannot_snap_label_view_->IsDrawn());
  EXPECT_TRUE(unsnappable_selector_item->cannot_snap_label_view_->IsDrawn());
}

// Test that when splitview mode and overview mode are both active at the same
// time, dragging divider behaviors are correct.
TEST_F(SplitViewWindowSelectorTest, DragDividerToExitTest) {
  UpdateDisplay("907x407");

  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();

  // Drag |window1| selector item to snap to left.
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  // Test that overview mode and split view mode are both active.
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Drag the divider toward closing the snapped window.
  gfx::Rect divider_bounds = GetSplitViewDividerBounds(false /* is_dragging */);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  split_view_controller()->EndResize(gfx::Point(0, 0));

  // Test that split view mode is ended. Overview mode is still active.
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Now drag |window2| selector item to snap to left.
  WindowSelectorItem* selector_item2 =
      GetWindowItemForWindow(grid_index, window2.get());
  DragWindowTo(selector_item2, gfx::Point(0, 0));
  // Test that overview mode and split view mode are both active.
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Drag the divider toward closing the overview window grid.
  divider_bounds = GetSplitViewDividerBounds(false /*is_dragging=*/);
  const gfx::Rect display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  split_view_controller()->EndResize(display_bounds.bottom_right());

  // Test that split view mode is ended. Overview mode is also ended. |window2|
  // should be activated.
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(window2.get(), wm::GetActiveWindow());
}

TEST_F(SplitViewWindowSelectorTest, WindowSelectorItemLongPressed) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  WindowSelectorItem* selector_item = GetWindowItemForWindow(0, window1.get());
  gfx::Point start_location(selector_item->target_bounds().CenterPoint());
  const gfx::Rect original_bounds(selector_item->target_bounds());

  // Verify that when a window selector item receives a resetting gesture, we
  // stay in overview mode and the bounds of the item are the same as they were
  // before the press sequence started.
  window_selector()->InitiateDrag(selector_item, start_location);
  window_selector()->ResetDraggedWindowGesture();
  EXPECT_TRUE(window_selector_controller()->IsSelecting());
  EXPECT_EQ(original_bounds, selector_item->target_bounds());

  // Verify that when a window selector item is tapped, we exit overview mode,
  // and the current active window is the item.
  window_selector()->InitiateDrag(selector_item, start_location);
  window_selector()->ActivateDraggedWindow();
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
  EXPECT_EQ(window1.get(), wm::GetActiveWindow());
}

TEST_F(SplitViewWindowSelectorTest, SnappedWindowBoundsTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  const int kMinimumBoundSize = 100;
  const gfx::Size size(kMinimumBoundSize, kMinimumBoundSize);

  std::unique_ptr<aura::Window> window1(
      CreateWindowWithMinimumSize(bounds, size));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithMinimumSize(bounds, size));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithMinimumSize(bounds, size));
  ToggleOverview();

  // Drag |window1| selector item to snap to left.
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Then drag the divider to left toward closing the snapped window.
  gfx::Rect divider_bounds = GetSplitViewDividerBounds(false /*is_dragging=*/);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  // Drag the divider to a point that is close enough but still have a short
  // distance to the edge of the screen.
  split_view_controller()->EndResize(gfx::Point(20, 20));

  // Test that split view mode is ended. Overview mode is still active.
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  // Test that |window1| has been moved out of the work area and is invisible.
  EXPECT_LE(window1->bounds().x(), -kMinimumBoundSize);
  EXPECT_EQ(window1->bounds().width(), kMinimumBoundSize);

  // Drag |window2| selector item to snap to right.
  WindowSelectorItem* selector_item2 =
      GetWindowItemForWindow(grid_index, window2.get());
  const gfx::Rect work_area_rect =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  gfx::Point end_location2 =
      gfx::Point(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(selector_item2, end_location2);
  EXPECT_EQ(SplitViewController::RIGHT_SNAPPED,
            split_view_controller()->state());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Then drag the divider to right toward closing the snapped window.
  divider_bounds = GetSplitViewDividerBounds(false /* is_dragging */);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  // Drag the divider to a point that is close enough but still have a short
  // distance to the edge of the screen.
  end_location2.Offset(-20, -20);
  split_view_controller()->EndResize(end_location2);

  // Test that split view mode is ended. Overview mode is still active.
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  // Test that |window2| has been moved out of the work area and is invisible.
  EXPECT_GE(window2->bounds().x(), work_area_rect.x());
  EXPECT_EQ(window2->bounds().width(), kMinimumBoundSize);
}

}  // namespace ash
