// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/drag_window_resizer.h"

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/wm/drag_window_controller.h"
#include "ash/wm/window_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

const int kRootHeight = 600;

}  // namespace

class DragWindowResizerTest : public test::AshTestBase {
 public:
  DragWindowResizerTest() {}
  virtual ~DragWindowResizerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    UpdateDisplay(base::StringPrintf("800x%d", kRootHeight));

    aura::Window* root = Shell::GetPrimaryRootWindow();
    gfx::Rect root_bounds(root->bounds());
    EXPECT_EQ(kRootHeight, root_bounds.height());
    EXPECT_EQ(800, root_bounds.width());
    Shell::GetInstance()->SetDisplayWorkAreaInsets(root, gfx::Insets());
    window_.reset(new aura::Window(&delegate_));
    window_->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    window_->Init(aura::WINDOW_LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(window_.get());
    window_->set_id(1);

    always_on_top_window_.reset(new aura::Window(&delegate2_));
    always_on_top_window_->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    always_on_top_window_->SetProperty(aura::client::kAlwaysOnTopKey, true);
    always_on_top_window_->Init(aura::WINDOW_LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(always_on_top_window_.get());
    always_on_top_window_->set_id(2);

    system_modal_window_.reset(new aura::Window(&delegate3_));
    system_modal_window_->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    system_modal_window_->SetProperty(aura::client::kModalKey,
                                      ui::MODAL_TYPE_SYSTEM);
    system_modal_window_->Init(aura::WINDOW_LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(system_modal_window_.get());
    system_modal_window_->set_id(3);

    transient_child_ = new aura::Window(&delegate4_);
    transient_child_->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    transient_child_->Init(aura::WINDOW_LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(transient_child_);
    transient_child_->set_id(4);

    transient_parent_.reset(new aura::Window(&delegate5_));
    transient_parent_->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    transient_parent_->Init(aura::WINDOW_LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(transient_parent_.get());
    ::wm::AddTransientChild(transient_parent_.get(), transient_child_);
    transient_parent_->set_id(5);

    panel_window_.reset(new aura::Window(&delegate6_));
    panel_window_->SetType(ui::wm::WINDOW_TYPE_PANEL);
    panel_window_->Init(aura::WINDOW_LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(panel_window_.get());
  }

  virtual void TearDown() OVERRIDE {
    window_.reset();
    always_on_top_window_.reset();
    system_modal_window_.reset();
    transient_parent_.reset();
    panel_window_.reset();
    AshTestBase::TearDown();
  }

 protected:
  gfx::Point CalculateDragPoint(const WindowResizer& resizer,
                                int delta_x,
                                int delta_y) const {
    gfx::Point location = resizer.GetInitialLocation();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    return location;
  }

  ShelfLayoutManager* shelf_layout_manager() {
    return Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
  }

  static WindowResizer* CreateDragWindowResizer(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component) {
    return CreateWindowResizer(
        window,
        point_in_parent,
        window_component,
        aura::client::WINDOW_MOVE_SOURCE_MOUSE).release();
  }

  bool WarpMouseCursorIfNecessary(aura::Window* target_root,
                                  const gfx::Point& point_in_screen) {
    MouseCursorEventFilter* event_filter =
        Shell::GetInstance()->mouse_cursor_filter();
    bool is_warped = event_filter->WarpMouseCursorIfNecessaryForTest(
        target_root, point_in_screen);
    event_filter->reset_was_mouse_warped_for_test();
    return is_warped;
  }

  aura::test::TestWindowDelegate delegate_;
  aura::test::TestWindowDelegate delegate2_;
  aura::test::TestWindowDelegate delegate3_;
  aura::test::TestWindowDelegate delegate4_;
  aura::test::TestWindowDelegate delegate5_;
  aura::test::TestWindowDelegate delegate6_;

  scoped_ptr<aura::Window> window_;
  scoped_ptr<aura::Window> always_on_top_window_;
  scoped_ptr<aura::Window> system_modal_window_;
  scoped_ptr<aura::Window> panel_window_;
  aura::Window* transient_child_;
  scoped_ptr<aura::Window> transient_parent_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DragWindowResizerTest);
};

// Verifies a window can be moved from the primary display to another.
TEST_F(DragWindowResizerTest, WindowDragWithMultiDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  // The secondary display is logically on the right, but on the system (e.g. X)
  // layer, it's below the primary one. See UpdateDisplay() in ash_test_base.cc.
  UpdateDisplay("800x600,400x300");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             Shell::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  {
    // Grab (0, 0) of the window.
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    // Drag the pointer to the right. Once it reaches the right edge of the
    // primary display, it warps to the secondary.
    resizer->Drag(CalculateDragPoint(*resizer, 800, 10), 0);
    resizer->CompleteDrag();
    // The whole window is on the secondary display now. The parent should be
    // changed.
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    EXPECT_EQ("0,10 50x60", window_->bounds().ToString());
  }

  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             Shell::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  {
    // Grab (0, 0) of the window and move the pointer to (790, 10).
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 795, 10), 0);
    // Window should be adjusted for minimum visibility (10px) during the drag.
    EXPECT_EQ("790,10 50x60", window_->bounds().ToString());
    resizer->CompleteDrag();
    // Since the pointer is still on the primary root window, the parent should
    // not be changed.
    // Window origin should be adjusted for minimum visibility (10px).
    EXPECT_EQ(root_windows[0], window_->GetRootWindow());
    EXPECT_EQ("790,10 50x60", window_->bounds().ToString());
  }

  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             Shell::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  {
    // Grab the top-right edge of the window and move the pointer to (0, 10)
    // in the secondary root window's coordinates.
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(49, 0), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 751, 10), ui::EF_CONTROL_DOWN);
    resizer->CompleteDrag();
    // Since the pointer is on the secondary, the parent should be changed
    // even though only small fraction of the window is within the secondary
    // root window's bounds.
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    // Window origin should be adjusted for minimum visibility (10px).
    int expected_x = -50 + 10;
    EXPECT_EQ(base::IntToString(expected_x) + ",10 50x60",
              window_->bounds().ToString());
  }
  // Dropping a window that is larger than the destination work area
  // will shrink to fit to the work area.
  window_->SetBoundsInScreen(gfx::Rect(0, 0, 700, 500),
                             Shell::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  {
    // Grab the top-right edge of the window and move the pointer to (0, 10)
    // in the secondary root window's coordinates.
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(699, 0), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 101, 10), ui::EF_CONTROL_DOWN);
    resizer->CompleteDrag();
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    // Window size should be adjusted to fit to the work area
    EXPECT_EQ("400x253", window_->bounds().size().ToString());
    gfx::Rect window_bounds_in_screen = window_->GetBoundsInScreen();
    gfx::Rect intersect(window_->GetRootWindow()->GetBoundsInScreen());
    intersect.Intersect(window_bounds_in_screen);

    EXPECT_LE(10, intersect.width());
    EXPECT_LE(10, intersect.height());
    EXPECT_TRUE(window_bounds_in_screen.Contains(gfx::Point(800, 10)));
  }

  // Dropping a window that is larger than the destination work area
  // will shrink to fit to the work area.
  window_->SetBoundsInScreen(gfx::Rect(0, 0, 700, 500),
                             Shell::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  {
    // Grab the top-left edge of the window and move the pointer to (150, 10)
    // in the secondary root window's coordinates. Make sure the window is
    // shrink in such a way that it keeps the cursor within.
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(0, 0), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 799, 10), ui::EF_CONTROL_DOWN);
    resizer->Drag(CalculateDragPoint(*resizer, 850, 10), ui::EF_CONTROL_DOWN);
    resizer->CompleteDrag();
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    // Window size should be adjusted to fit to the work area
    EXPECT_EQ("400x253", window_->bounds().size().ToString());
    gfx::Rect window_bounds_in_screen = window_->GetBoundsInScreen();
    gfx::Rect intersect(window_->GetRootWindow()->GetBoundsInScreen());
    intersect.Intersect(window_bounds_in_screen);
    EXPECT_LE(10, intersect.width());
    EXPECT_LE(10, intersect.height());
    EXPECT_TRUE(window_bounds_in_screen.Contains(gfx::Point(850, 10)));
  }
}

// Verifies that dragging the active window to another display makes the new
// root window the active root window.
TEST_F(DragWindowResizerTest, WindowDragWithMultiDisplaysActiveRoot) {
  if (!SupportsMultipleDisplays())
    return;

  // The secondary display is logically on the right, but on the system (e.g. X)
  // layer, it's below the primary one. See UpdateDisplay() in ash_test_base.cc.
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(new aura::Window(&delegate));
  window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window->Init(aura::WINDOW_LAYER_TEXTURED);
  ParentWindowInPrimaryRootWindow(window.get());
  window->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                            Shell::GetScreen()->GetPrimaryDisplay());
  window->Show();
  EXPECT_TRUE(ash::wm::CanActivateWindow(window.get()));
  ash::wm::ActivateWindow(window.get());
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ(root_windows[0], ash::Shell::GetTargetRootWindow());
  {
    // Grab (0, 0) of the window.
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    // Drag the pointer to the right. Once it reaches the right edge of the
    // primary display, it warps to the secondary.
    resizer->Drag(CalculateDragPoint(*resizer, 800, 10), 0);
    resizer->CompleteDrag();
    // The whole window is on the secondary display now. The parent should be
    // changed.
    EXPECT_EQ(root_windows[1], window->GetRootWindow());
    EXPECT_EQ(root_windows[1], ash::Shell::GetTargetRootWindow());
  }
}

// Verifies a window can be moved from the secondary display to primary.
TEST_F(DragWindowResizerTest, WindowDragWithMultiDisplaysRightToLeft) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  window_->SetBoundsInScreen(
      gfx::Rect(800, 00, 50, 60),
      Shell::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
  EXPECT_EQ(root_windows[1], window_->GetRootWindow());
  {
    // Grab (0, 0) of the window.
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    // Move the mouse near the right edge, (798, 0), of the primary display.
    resizer->Drag(CalculateDragPoint(*resizer, -2, 0), ui::EF_CONTROL_DOWN);
    resizer->CompleteDrag();
    EXPECT_EQ(root_windows[0], window_->GetRootWindow());
    // Window origin should be adjusted for minimum visibility (10px).
    int expected_x = 800 - 10;
    EXPECT_EQ(base::IntToString(expected_x) + ",0 50x60",
              window_->bounds().ToString());
  }
}

// Verifies the drag window is shown correctly.
TEST_F(DragWindowResizerTest, DragWindowController) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             Shell::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  {
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    DragWindowResizer* drag_resizer = DragWindowResizer::instance_;
    ASSERT_TRUE(drag_resizer);
    EXPECT_FALSE(drag_resizer->drag_window_controller_.get());

    // The pointer is inside the primary root. The drag window controller
    // should be NULL.
    resizer->Drag(CalculateDragPoint(*resizer, 10, 10), 0);
    EXPECT_FALSE(drag_resizer->drag_window_controller_.get());

    // The window spans both root windows.
    resizer->Drag(CalculateDragPoint(*resizer, 798, 10), 0);
    DragWindowController* controller =
        drag_resizer->drag_window_controller_.get();
    ASSERT_TRUE(controller);

    ASSERT_TRUE(controller->drag_widget_);
    ui::Layer* drag_layer =
        controller->drag_widget_->GetNativeWindow()->layer();
    ASSERT_TRUE(drag_layer);
    // Check if |resizer->layer_| is properly set to the drag widget.
    const std::vector<ui::Layer*>& layers = drag_layer->children();
    EXPECT_FALSE(layers.empty());
    EXPECT_EQ(controller->layer_owner_->root(), layers.back());

    // |window_| should be opaque since the pointer is still on the primary
    // root window. The drag window should be semi-transparent.
    EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
    ASSERT_TRUE(controller->drag_widget_);
    EXPECT_GT(1.0f, drag_layer->opacity());

    // Enter the pointer to the secondary display.
    resizer->Drag(CalculateDragPoint(*resizer, 800, 10), 0);
    controller = drag_resizer->drag_window_controller_.get();
    ASSERT_TRUE(controller);
    // |window_| should be transparent, and the drag window should be opaque.
    EXPECT_GT(1.0f, window_->layer()->opacity());
    EXPECT_FLOAT_EQ(1.0f, drag_layer->opacity());

    resizer->CompleteDrag();
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  }

  // Do the same test with RevertDrag().
  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             Shell::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  {
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    DragWindowResizer* drag_resizer = DragWindowResizer::instance_;
    ASSERT_TRUE(drag_resizer);
    EXPECT_FALSE(drag_resizer->drag_window_controller_.get());

    resizer->Drag(CalculateDragPoint(*resizer, 0, 610), 0);
    resizer->RevertDrag();
    EXPECT_EQ(root_windows[0], window_->GetRootWindow());
    EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  }
}

// Verifies if the resizer sets and resets
// MouseCursorEventFilter::mouse_warp_mode_ as expected.
TEST_F(DragWindowResizerTest, WarpMousePointer) {
  MouseCursorEventFilter* event_filter =
      Shell::GetInstance()->mouse_cursor_filter();
  ASSERT_TRUE(event_filter);
  window_->SetBounds(gfx::Rect(0, 0, 50, 60));

  EXPECT_EQ(MouseCursorEventFilter::WARP_ALWAYS,
            event_filter->mouse_warp_mode_);
  {
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    // While dragging a window, warp should be allowed.
    EXPECT_EQ(MouseCursorEventFilter::WARP_DRAG,
              event_filter->mouse_warp_mode_);
    resizer->CompleteDrag();
  }
  EXPECT_EQ(MouseCursorEventFilter::WARP_ALWAYS,
            event_filter->mouse_warp_mode_);

  {
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    EXPECT_EQ(MouseCursorEventFilter::WARP_DRAG,
              event_filter->mouse_warp_mode_);
    resizer->RevertDrag();
  }
  EXPECT_EQ(MouseCursorEventFilter::WARP_ALWAYS,
            event_filter->mouse_warp_mode_);

  {
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTRIGHT));
    // While resizing a window, warp should NOT be allowed.
    EXPECT_EQ(MouseCursorEventFilter::WARP_NONE,
              event_filter->mouse_warp_mode_);
    resizer->CompleteDrag();
  }
  EXPECT_EQ(MouseCursorEventFilter::WARP_ALWAYS,
            event_filter->mouse_warp_mode_);

  {
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTRIGHT));
    EXPECT_EQ(MouseCursorEventFilter::WARP_NONE,
              event_filter->mouse_warp_mode_);
    resizer->RevertDrag();
  }
  EXPECT_EQ(MouseCursorEventFilter::WARP_ALWAYS,
            event_filter->mouse_warp_mode_);
}

// Verifies cursor's device scale factor is updated whe a window is moved across
// root windows with different device scale factors (http://crbug.com/154183).
TEST_F(DragWindowResizerTest, CursorDeviceScaleFactor) {
  if (!SupportsMultipleDisplays())
    return;

  // The secondary display is logically on the right, but on the system (e.g. X)
  // layer, it's below the primary one. See UpdateDisplay() in ash_test_base.cc.
  UpdateDisplay("400x400,800x800*2");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  test::CursorManagerTestApi cursor_test_api(
      Shell::GetInstance()->cursor_manager());
  // Move window from the root window with 1.0 device scale factor to the root
  // window with 2.0 device scale factor.
  {
    window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                               Shell::GetScreen()->GetPrimaryDisplay());
    EXPECT_EQ(root_windows[0], window_->GetRootWindow());
    // Grab (0, 0) of the window.
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    EXPECT_EQ(1.0f, cursor_test_api.GetCurrentCursor().device_scale_factor());
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    WarpMouseCursorIfNecessary(root_windows[0], gfx::Point(399, 200));
    EXPECT_EQ(2.0f, cursor_test_api.GetCurrentCursor().device_scale_factor());
    resizer->CompleteDrag();
    EXPECT_EQ(2.0f, cursor_test_api.GetCurrentCursor().device_scale_factor());
  }

  // Move window from the root window with 2.0 device scale factor to the root
  // window with 1.0 device scale factor.
  {
    // Make sure the window is on the default container first.
    aura::Window* default_container =
        GetRootWindowController(root_windows[1])
            ->GetContainer(kShellWindowId_DefaultContainer);
    default_container->AddChild(window_.get());
    window_->SetBoundsInScreen(
        gfx::Rect(600, 0, 50, 60),
        Shell::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    // Grab (0, 0) of the window.
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    EXPECT_EQ(2.0f, cursor_test_api.GetCurrentCursor().device_scale_factor());
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, -200, 200), 0);
    WarpMouseCursorIfNecessary(root_windows[1], gfx::Point(400, 200));
    EXPECT_EQ(1.0f, cursor_test_api.GetCurrentCursor().device_scale_factor());
    resizer->CompleteDrag();
    EXPECT_EQ(1.0f, cursor_test_api.GetCurrentCursor().device_scale_factor());
  }
}

// Verifies several kinds of windows can be moved across displays.
TEST_F(DragWindowResizerTest, MoveWindowAcrossDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  // The secondary display is logically on the right, but on the system (e.g. X)
  // layer, it's below the primary one. See UpdateDisplay() in ash_test_base.cc.
  UpdateDisplay("400x400,400x400");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  // Normal window can be moved across display.
  {
    aura::Window* window = window_.get();
    window->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                              Shell::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_TRUE(WarpMouseCursorIfNecessary(root_windows[0],
                                           gfx::Point(399, 200)));
    resizer->CompleteDrag();
  }

  // Always on top window can be moved across display.
  {
    aura::Window* window = always_on_top_window_.get();
    window->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                              Shell::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_TRUE(WarpMouseCursorIfNecessary(root_windows[0],
                                           gfx::Point(399, 200)));
    resizer->CompleteDrag();
  }

  // System modal window can be moved across display.
  {
    aura::Window* window = system_modal_window_.get();
    window->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                              Shell::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_TRUE(WarpMouseCursorIfNecessary(root_windows[0],
                                           gfx::Point(399, 200)));
    resizer->CompleteDrag();
  }

  // Transient window cannot be moved across display.
  {
    aura::Window* window = transient_child_;
    window->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                              Shell::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_FALSE(WarpMouseCursorIfNecessary(
        root_windows[0],
        gfx::Point(399, 200)));
    resizer->CompleteDrag();
  }

  // The parent of transient window can be moved across display.
  {
    aura::Window* window = transient_parent_.get();
    window->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                              Shell::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_TRUE(WarpMouseCursorIfNecessary(root_windows[0],
                                           gfx::Point(399, 200)));
    resizer->CompleteDrag();
  }

  // Panel window can be moved across display.
  {
    aura::Window* window = panel_window_.get();
    window->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                              Shell::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    scoped_ptr<WindowResizer> resizer(CreateDragWindowResizer(
        window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_TRUE(WarpMouseCursorIfNecessary(root_windows[0],
                                           gfx::Point(399, 200)));
    resizer->CompleteDrag();
  }
}

}  // namespace ash
