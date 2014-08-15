// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "ash/accessibility_delegate.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shelf_test_api.h"
#include "ash/test/shelf_view_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_shelf_delegate.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_delegate.h"

namespace ash {
namespace {

class NonActivatableActivationDelegate
    : public aura::client::ActivationDelegate {
 public:
  virtual bool ShouldActivate() const OVERRIDE {
    return false;
  }
};

void CancelDrag(DragDropController* controller, bool* canceled) {
  if (controller->IsDragDropInProgress()) {
    *canceled = true;
    controller->DragCancel();
  }
}

}  // namespace

class WindowSelectorTest : public test::AshTestBase {
 public:
  WindowSelectorTest() {}
  virtual ~WindowSelectorTest() {}

  virtual void SetUp() OVERRIDE {
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

  aura::Window* CreatePanelWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        NULL, ui::wm::WINDOW_TYPE_PANEL, 0, bounds);
    test::TestShelfDelegate::instance()->AddShelfItem(window);
    shelf_view_test()->RunMessageLoopUntilAnimationsDone();
    return window;
  }

  views::Widget* CreatePanelWindowWidget(const gfx::Rect& bounds) {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params;
    params.bounds = bounds;
    params.type = views::Widget::InitParams::TYPE_PANEL;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget->Init(params);
    widget->Show();
    ParentWindowInPrimaryRootWindow(widget->GetNativeWindow());
    return widget;
  }

  bool WindowsOverlapping(aura::Window* window1, aura::Window* window2) {
    gfx::RectF window1_bounds = GetTransformedTargetBounds(window1);
    gfx::RectF window2_bounds = GetTransformedTargetBounds(window2);
    return window1_bounds.Intersects(window2_bounds);
  }

  void ToggleOverview() {
    ash::Shell::GetInstance()->window_selector_controller()->ToggleOverview();
  }

  gfx::Transform GetTransformRelativeTo(gfx::PointF origin,
                                        const gfx::Transform& transform) {
    gfx::Transform t;
    t.Translate(origin.x(), origin.y());
    t.PreconcatTransform(transform);
    t.Translate(-origin.x(), -origin.y());
    return t;
  }

  gfx::RectF GetTransformedBounds(aura::Window* window) {
    gfx::RectF bounds(ScreenUtil::ConvertRectToScreen(
        window->parent(), window->layer()->bounds()));
    gfx::Transform transform(GetTransformRelativeTo(bounds.origin(),
        window->layer()->transform()));
    transform.TransformRect(&bounds);
    return bounds;
  }

  gfx::RectF GetTransformedTargetBounds(aura::Window* window) {
    gfx::RectF bounds(ScreenUtil::ConvertRectToScreen(
        window->parent(), window->layer()->GetTargetBounds()));
    gfx::Transform transform(GetTransformRelativeTo(bounds.origin(),
        window->layer()->GetTargetTransform()));
    transform.TransformRect(&bounds);
    return bounds;
  }

  gfx::RectF GetTransformedBoundsInRootWindow(aura::Window* window) {
    gfx::RectF bounds = gfx::Rect(window->bounds().size());
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

  const aura::Window* GetSelectedWindow() {
    WindowSelector* ws = ash::Shell::GetInstance()->
        window_selector_controller()->window_selector_.get();
    return ws->grid_list_[ws->selected_grid_index_]->
        SelectedWindow()->SelectionWindow();
  }

  const bool selection_widget_active() {
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
    return window->close_button_.get();
  }

  views::Label* GetLabelView(ash::WindowSelectorItem* window) {
    return window->window_label_view_;
  }

  // Tests that a window is contained within a given WindowSelectorItem, and
  // that both the window and its matching close button are within the same
  // screen.
  void IsWindowAndCloseButtonInScreen(aura::Window* window,
                                      WindowSelectorItem* window_item) {
    aura::Window* root_window = window_item->GetRootWindow();
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
            ContentsChanged(NULL, base::UTF8ToUTF16(pattern));
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
  scoped_ptr<test::ShelfViewTestAPI> shelf_view_test_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorTest);
};

// Tests that an a11y alert is sent on entering overview mode.
TEST_F(WindowSelectorTest, A11yAlertOnOverviewMode) {
  gfx::Rect bounds(0, 0, 400, 400);
  AccessibilityDelegate* delegate =
      ash::Shell::GetInstance()->accessibility_delegate();
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  EXPECT_NE(delegate->GetLastAccessibilityAlert(),
            A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED);
  ToggleOverview();
  EXPECT_EQ(delegate->GetLastAccessibilityAlert(),
            A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED);
}

// Tests entering overview mode with two windows and selecting one by clicking.
TEST_F(WindowSelectorTest, Basic) {
  gfx::Rect bounds(0, 0, 400, 400);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  scoped_ptr<aura::Window> panel1(CreatePanelWindow(bounds));
  scoped_ptr<aura::Window> panel2(CreatePanelWindow(bounds));
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
  // Panels 1 and 2 should still be overlapping being in a single selector
  // item.
  EXPECT_TRUE(WindowsOverlapping(panel1.get(), panel2.get()));

  // Clicking window 1 should activate it.
  ClickWindow(window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  EXPECT_EQ(window1.get(), GetFocusedWindow());

  // Cursor should have been unlocked.
  EXPECT_FALSE(aura::client::GetCursorClient(root_window)->IsCursorLocked());
}

// Tests selecting a window by tapping on it.
TEST_F(WindowSelectorTest, BasicGesture) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
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

// Tests that a window does not receive located events when in overview mode.
TEST_F(WindowSelectorTest, WindowDoesNotReceiveEvents) {
  gfx::Rect window_bounds(20, 10, 200, 300);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  scoped_ptr<aura::Window> window(CreateWindow(window_bounds));

  gfx::Point point1(window_bounds.x() + 10, window_bounds.y() + 10);

  ui::MouseEvent event1(ui::ET_MOUSE_PRESSED, point1, point1,
                        ui::EF_NONE, ui::EF_NONE);

  ui::EventTarget* root_target = root_window;
  ui::EventTargeter* targeter = root_target->GetEventTargeter();

  // The event should target the window because we are still not in overview
  // mode.
  EXPECT_EQ(window, static_cast<aura::Window*>(
      targeter->FindTargetForEvent(root_target, &event1)));

  ToggleOverview();

  // The bounds have changed, take that into account.
  gfx::RectF bounds = GetTransformedBoundsInRootWindow(window.get());
  gfx::Point point2(bounds.x() + 10, bounds.y() + 10);
  ui::MouseEvent event2(ui::ET_MOUSE_PRESSED, point2, point2,
                        ui::EF_NONE, ui::EF_NONE);

  // Now the transparent window should be intercepting this event.
  EXPECT_NE(window, static_cast<aura::Window*>(
        targeter->FindTargetForEvent(root_target, &event2)));
}

// Tests that clicking on the close button effectively closes the window.
TEST_F(WindowSelectorTest, CloseButton) {
  scoped_ptr<aura::Window> window1(CreateWindow(gfx::Rect(200, 300, 250, 450)));

  // We need a widget for the close button the work, a bare window will crash.
  scoped_ptr<views::Widget> widget(new views::Widget);
  views::Widget::InitParams params;
  params.bounds = gfx::Rect(0, 0, 400, 400);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = window1->parent();
  widget->Init(params);
  widget->Show();
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
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  scoped_ptr<aura::Window> panel1(CreatePanelWindow(bounds));
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
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  wm::WindowState* window_state = wm::GetWindowState(window1.get());
  window_state->Maximize();
  ash::ShelfWidget* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  EXPECT_TRUE(shelf->GetDimsShelf());
  ToggleOverview();
  EXPECT_FALSE(shelf->GetDimsShelf());
  ToggleOverview();
  EXPECT_TRUE(shelf->GetDimsShelf());
}

// Tests that beginning window selection hides the app list.
TEST_F(WindowSelectorTest, SelectingHidesAppList) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  Shell::GetInstance()->ShowAppList(NULL);
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
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
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
  scoped_ptr<aura::Window> window(CreateWindow(gfx::Rect(0, 0, 400, 400)));
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
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  ToggleOverview();
  EXPECT_TRUE(IsSelecting());

  // A window being created should exit overview mode.
  scoped_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_FALSE(IsSelecting());
}

// Tests that a window activation exits overview mode.
TEST_F(WindowSelectorTest, ActivationCancelsOveriew) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
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
  scoped_ptr<aura::Window> window(CreateWindow(bounds));
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
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  ToggleOverview();

  window1.reset();
  window2.reset();
  EXPECT_FALSE(IsSelecting());
}

// Tests that entering overview mode restores a window to its original
// target location.
TEST_F(WindowSelectorTest, QuickReentryRestoresInitialTransform) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window(CreateWindow(bounds));
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

// Tests that non-activatable windows are hidden when entering overview mode.
TEST_F(WindowSelectorTest, NonActivatableWindowsHidden) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  scoped_ptr<aura::Window> non_activatable_window(
      CreateNonActivatableWindow(Shell::GetPrimaryRootWindow()->bounds()));
  EXPECT_TRUE(non_activatable_window->IsVisible());
  ToggleOverview();
  EXPECT_FALSE(non_activatable_window->IsVisible());
  ToggleOverview();
  EXPECT_TRUE(non_activatable_window->IsVisible());

  // Test that a window behind the fullscreen non-activatable window can be
  // clicked.
  non_activatable_window->parent()->StackChildAtTop(
      non_activatable_window.get());
  ToggleOverview();
  ClickWindow(window1.get());
  EXPECT_FALSE(IsSelecting());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
}

// Tests that windows with modal child windows are transformed with the modal
// child even though not activatable themselves.
TEST_F(WindowSelectorTest, ModalChild) {
  gfx::Rect bounds(0, 0, 400, 400);
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> child1(CreateWindow(bounds));
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
  scoped_ptr<aura::Window> window1(CreateWindow(gfx::Rect(0, 0, 180, 180)));
  scoped_ptr<aura::Window> child1(CreateWindow(gfx::Rect(200, 0, 180, 180)));
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

  scoped_ptr<aura::Window> window1(CreateWindow(bounds1));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds1));
  scoped_ptr<aura::Window> window3(CreateWindow(bounds2));
  scoped_ptr<aura::Window> window4(CreateWindow(bounds2));
  scoped_ptr<aura::Window> panel1(CreatePanelWindow(bounds1));
  scoped_ptr<aura::Window> panel2(CreatePanelWindow(bounds1));
  scoped_ptr<aura::Window> panel3(CreatePanelWindow(bounds2));
  scoped_ptr<aura::Window> panel4(CreatePanelWindow(bounds2));
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

  const std::vector<WindowSelectorItem*>& primary_window_items =
      GetWindowItemsForRoot(0);
  const std::vector<WindowSelectorItem*>& secondary_window_items =
      GetWindowItemsForRoot(1);

  // Window indices are based on top-down order. The reverse of our creation.
  IsWindowAndCloseButtonInScreen(window1.get(), primary_window_items[2]);
  IsWindowAndCloseButtonInScreen(window2.get(), primary_window_items[1]);
  IsWindowAndCloseButtonInScreen(window3.get(), secondary_window_items[2]);
  IsWindowAndCloseButtonInScreen(window4.get(), secondary_window_items[1]);

  IsWindowAndCloseButtonInScreen(panel1.get(), primary_window_items[0]);
  IsWindowAndCloseButtonInScreen(panel2.get(), primary_window_items[0]);
  IsWindowAndCloseButtonInScreen(panel3.get(), secondary_window_items[0]);
  IsWindowAndCloseButtonInScreen(panel4.get(), secondary_window_items[0]);

  EXPECT_TRUE(WindowsOverlapping(panel1.get(), panel2.get()));
  EXPECT_TRUE(WindowsOverlapping(panel3.get(), panel4.get()));
  EXPECT_FALSE(WindowsOverlapping(panel1.get(), panel3.get()));
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
  scoped_ptr<aura::Window> window1(CreateWindow(bounds1));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds2));
  scoped_ptr<aura::Window> window3(CreatePanelWindow(bounds1));
  scoped_ptr<aura::Window> window4(CreatePanelWindow(bounds2));

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
  scoped_ptr<aura::Window> window(CreateWindow(bounds));
  test::ShellTestApi shell_test_api(Shell::GetInstance());
  ash::DragDropController* drag_drop_controller =
      shell_test_api.drag_drop_controller();
  ui::OSExchangeData data;
  base::MessageLoopForUI::current()->PostTask(FROM_HERE,
      base::Bind(&WindowSelectorTest::ToggleOverview,
                 base::Unretained(this)));
  base::MessageLoopForUI::current()->PostTask(FROM_HERE,
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
  scoped_ptr<aura::Window> window(CreateWindow(gfx::Rect(0, 0, 100, 100)));
  base::string16 window_title = base::UTF8ToUTF16("My window");
  window->SetTitle(window_title);
  ToggleOverview();
  WindowSelectorItem* window_item = GetWindowItemsForRoot(0).back();
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
  gfx::Rect target_bounds(window_item->target_bounds());
  gfx::Rect expected_label_bounds(target_bounds.x(),
                                  target_bounds.bottom() - label->
                                      GetPreferredSize().height(),
                                  target_bounds.width(),
                                  label->GetPreferredSize().height());
  gfx::Rect real_label_bounds = label->GetWidget()->GetNativeWindow()->bounds();
  EXPECT_EQ(real_label_bounds, expected_label_bounds);
}

// Tests that a label is created for the active panel in a group of panels in
// overview mode.
TEST_F(WindowSelectorTest, CreateLabelUnderPanel) {
  scoped_ptr<aura::Window> panel1(CreatePanelWindow(gfx::Rect(0, 0, 100, 100)));
  scoped_ptr<aura::Window> panel2(CreatePanelWindow(gfx::Rect(0, 0, 100, 100)));
  base::string16 panel1_title = base::UTF8ToUTF16("My panel");
  base::string16 panel2_title = base::UTF8ToUTF16("Another panel");
  base::string16 updated_panel1_title = base::UTF8ToUTF16("WebDriver Torso");
  base::string16 updated_panel2_title = base::UTF8ToUTF16("Da panel");
  panel1->SetTitle(panel1_title);
  panel2->SetTitle(panel2_title);
  wm::ActivateWindow(panel1.get());
  ToggleOverview();
  WindowSelectorItem* window_item = GetWindowItemsForRoot(0).back();
  views::Label* label = GetLabelView(window_item);
  // Has the label view been created?
  ASSERT_TRUE(label);

  // Verify the label matches the active window title.
  EXPECT_EQ(label->text(), panel1_title);
  // Verify that updating the title also updates the label.
  panel1->SetTitle(updated_panel1_title);
  EXPECT_EQ(label->text(), updated_panel1_title);
  // After destroying the first panel, the label should match the second panel.
  panel1.reset();
  label = GetLabelView(window_item);
  EXPECT_EQ(label->text(), panel2_title);
  // Also test updating the title on the second panel.
  panel2->SetTitle(updated_panel2_title);
  EXPECT_EQ(label->text(), updated_panel2_title);
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
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  ToggleOverview();

  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(GetSelectedWindow(), window1.get());
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(GetSelectedWindow(), window2.get());
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(GetSelectedWindow(), window1.get());
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
    for (size_t i = 0; i < test_windows + 1; i++) {
      SendKey(arrow_keys[key_index]);
      // TODO(flackr): Add a more readable error message by constructing a
      // string from the window IDs.
      EXPECT_EQ(GetSelectedWindow()->id(),
                index_path_for_direction[key_index][i]);
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
  scoped_ptr<aura::Window> window4(CreateWindow(bounds2));
  scoped_ptr<aura::Window> window3(CreateWindow(bounds2));
  scoped_ptr<aura::Window> window2(CreateWindow(bounds1));
  scoped_ptr<aura::Window> window1(CreateWindow(bounds1));


  ToggleOverview();

  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), window1.get());
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), window2.get());
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), window3.get());
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), window4.get());
}

// Tests selecting a window in overview mode with the return key.
TEST_F(WindowSelectorTest, SelectWindowWithReturnKey) {
  gfx::Rect bounds(0, 0, 100, 100);
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  ToggleOverview();

  // Pressing the return key without a selection widget should not do anything.
  SendKey(ui::VKEY_RETURN);
  EXPECT_TRUE(IsSelecting());

  // Select the first window.
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_RETURN);
  ASSERT_FALSE(IsSelecting());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Select the second window.
  ToggleOverview();
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_RETURN);
  EXPECT_FALSE(IsSelecting());
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests that overview mode hides the callout widget.
TEST_F(WindowSelectorTest, WindowOverviewHidesCalloutWidgets) {
  scoped_ptr<aura::Window> panel1(CreatePanelWindow(gfx::Rect(0, 0, 100, 100)));
  scoped_ptr<aura::Window> panel2(CreatePanelWindow(gfx::Rect(0, 0, 100, 100)));
  PanelLayoutManager* panel_manager =
        static_cast<PanelLayoutManager*>(panel1->parent()->layout_manager());

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

// Tests that when panels are grouped that the close button only closes the
// currently active panel. After the removal window selection should still be
// active, and the label should have changed. Removing the last panel should
// cause selection to end.
TEST_F(WindowSelectorTest, CloseButtonOnPanels) {
  scoped_ptr<views::Widget> widget1(CreatePanelWindowWidget(
      gfx::Rect(0, 0, 300, 100)));
  scoped_ptr<views::Widget> widget2(CreatePanelWindowWidget(
      gfx::Rect(100, 0, 100, 100)));
  aura::Window* window1 = widget1->GetNativeWindow();
  aura::Window* window2 = widget2->GetNativeWindow();
  base::string16 panel1_title = base::UTF8ToUTF16("Panel 1");
  base::string16 panel2_title = base::UTF8ToUTF16("Panel 2");
  window1->SetTitle(panel1_title);
  window2->SetTitle(panel2_title);
  wm::ActivateWindow(window1);
  ToggleOverview();

  gfx::RectF bounds1 = GetTransformedBoundsInRootWindow(window1);
  gfx::Point point1(bounds1.top_right().x() - 1, bounds1.top_right().y() - 1);
  ui::test::EventGenerator event_generator1(window1->GetRootWindow(), point1);

  EXPECT_FALSE(widget1->IsClosed());
  event_generator1.ClickLeftButton();
  EXPECT_TRUE(widget1->IsClosed());
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(IsSelecting());
  WindowSelectorItem* window_item = GetWindowItemsForRoot(0).front();
  EXPECT_FALSE(window_item->empty());
  EXPECT_TRUE(window_item->Contains(window2));
  EXPECT_TRUE(GetCloseButton(window_item)->IsVisible());


  views::Label* label = GetLabelView(window_item);
  EXPECT_EQ(label->text(), panel2_title);

  gfx::RectF bounds2 = GetTransformedBoundsInRootWindow(window2);
  gfx::Point point2(bounds2.top_right().x() - 1, bounds2.top_right().y() - 1);
  ui::test::EventGenerator event_generator2(window2->GetRootWindow(), point2);

  EXPECT_FALSE(widget2->IsClosed());
  event_generator2.ClickLeftButton();
  EXPECT_TRUE(widget2->IsClosed());
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(IsSelecting());
}

// Creates three windows and tests filtering them by title.
TEST_F(WindowSelectorTest, BasicTextFiltering) {
  gfx::Rect bounds(0, 0, 100, 100);
  scoped_ptr<aura::Window> window2(CreateWindow(bounds));
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
  scoped_ptr<aura::Window> window0(CreateWindow(bounds));
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
  // selecting the first matching window.
  EXPECT_TRUE(selection_widget_active());
  EXPECT_TRUE(showing_filter_widget());
  EXPECT_EQ(GetSelectedWindow(), window1.get());

  // Window 0 has no "test" on it so it should be the only dimmed item.
  std::vector<WindowSelectorItem*> items = GetWindowItemsForRoot(0);
  EXPECT_TRUE(items[0]->dimmed());
  EXPECT_FALSE(items[1]->dimmed());
  EXPECT_FALSE(items[2]->dimmed());

  // No items match the search.
  FilterItems("I'm testing 'n testing");
  EXPECT_TRUE(items[0]->dimmed());
  EXPECT_TRUE(items[1]->dimmed());
  EXPECT_TRUE(items[2]->dimmed());

  // All the items should match the empty string. The filter widget should also
  // disappear.
  FilterItems("");
  EXPECT_FALSE(showing_filter_widget());
  EXPECT_FALSE(items[0]->dimmed());
  EXPECT_FALSE(items[1]->dimmed());
  EXPECT_FALSE(items[2]->dimmed());
}

// Tests selecting in the overview with dimmed and undimmed items.
TEST_F(WindowSelectorTest, TextFilteringSelection) {
  gfx::Rect bounds(0, 0, 100, 100);
   scoped_ptr<aura::Window> window2(CreateWindow(bounds));
   scoped_ptr<aura::Window> window1(CreateWindow(bounds));
   scoped_ptr<aura::Window> window0(CreateWindow(bounds));
   base::string16 window2_title = base::UTF8ToUTF16("Rock and roll");
   base::string16 window1_title = base::UTF8ToUTF16("Rock and");
   base::string16 window0_title = base::UTF8ToUTF16("Rock");
   window0->SetTitle(window0_title);
   window1->SetTitle(window1_title);
   window2->SetTitle(window2_title);
   ToggleOverview();
   SendKey(ui::VKEY_RIGHT);
   EXPECT_TRUE(selection_widget_active());
   EXPECT_EQ(GetSelectedWindow(), window0.get());

   // Dim the first item, the selection should jump to the next item.
   std::vector<WindowSelectorItem*> items = GetWindowItemsForRoot(0);
   FilterItems("Rock and");
   EXPECT_EQ(GetSelectedWindow(), window1.get());

   // Cycle the selection, the dimmed window should not be selected.
   SendKey(ui::VKEY_RIGHT);
   EXPECT_EQ(GetSelectedWindow(), window2.get());
   SendKey(ui::VKEY_RIGHT);
   EXPECT_EQ(GetSelectedWindow(), window1.get());

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
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
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
  scoped_ptr<aura::Window> window1(CreateWindow(bounds));
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
