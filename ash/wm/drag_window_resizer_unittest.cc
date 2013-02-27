// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/drag_window_resizer.h"

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/wm/drag_window_controller.h"
#include "ash/wm/shelf_layout_manager.h"
#include "base/stringprintf.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {
namespace {

const int kRootHeight = 600;

}  // namespace

class DragWindowResizerTest : public test::AshTestBase {
 public:
  DragWindowResizerTest() : window_(NULL) {}
  virtual ~DragWindowResizerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    UpdateDisplay(StringPrintf("800x%d", kRootHeight));

    aura::RootWindow* root = Shell::GetPrimaryRootWindow();
    gfx::Rect root_bounds(root->bounds());
    EXPECT_EQ(kRootHeight, root_bounds.height());
    EXPECT_EQ(800, root_bounds.width());
    Shell::GetInstance()->SetDisplayWorkAreaInsets(root, gfx::Insets());
    window_.reset(new aura::Window(&delegate_));
    window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window_->Init(ui::LAYER_NOT_DRAWN);
    SetDefaultParentByPrimaryRootWindow(window_.get());
    window_->set_id(1);

    always_on_top_window_.reset(new aura::Window(&delegate2_));
    always_on_top_window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    always_on_top_window_->SetProperty(aura::client::kAlwaysOnTopKey, true);
    always_on_top_window_->Init(ui::LAYER_NOT_DRAWN);
    SetDefaultParentByPrimaryRootWindow(always_on_top_window_.get());
    always_on_top_window_->set_id(2);

    system_modal_window_.reset(new aura::Window(&delegate3_));
    system_modal_window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    system_modal_window_->SetProperty(aura::client::kModalKey,
                                      ui::MODAL_TYPE_SYSTEM);
    system_modal_window_->Init(ui::LAYER_NOT_DRAWN);
    SetDefaultParentByPrimaryRootWindow(system_modal_window_.get());
    system_modal_window_->set_id(3);

    transient_child_ = new aura::Window(&delegate4_);
    transient_child_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    transient_child_->Init(ui::LAYER_NOT_DRAWN);
    SetDefaultParentByPrimaryRootWindow(transient_child_);
    transient_child_->set_id(4);

    transient_parent_.reset(new aura::Window(&delegate5_));
    transient_parent_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    transient_parent_->Init(ui::LAYER_NOT_DRAWN);
    SetDefaultParentByPrimaryRootWindow(transient_parent_.get());
    transient_parent_->AddTransientChild(transient_child_);
    transient_parent_->set_id(5);
  }

  virtual void TearDown() OVERRIDE {
    window_.reset();
    always_on_top_window_.reset();
    system_modal_window_.reset();
    transient_parent_.reset();
    AshTestBase::TearDown();
  }

 protected:
  gfx::Point CalculateDragPoint(const DragWindowResizer& resizer,
                                int delta_x,
                                int delta_y) const {
    gfx::Point location = resizer.GetInitialLocationInParentForTest();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    return location;
  }

  internal::ShelfLayoutManager* shelf_layout_manager() {
    return Shell::GetPrimaryRootWindowController()->shelf();
  }

  static DragWindowResizer* CreateDragWindowResizer(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component) {
    return static_cast<DragWindowResizer*>(CreateWindowResizer(
        window, point_in_parent, window_component).release());
  }

  aura::test::TestWindowDelegate delegate_;
  aura::test::TestWindowDelegate delegate2_;
  aura::test::TestWindowDelegate delegate3_;
  aura::test::TestWindowDelegate delegate4_;
  aura::test::TestWindowDelegate delegate5_;

  scoped_ptr<aura::Window> window_;
  scoped_ptr<aura::Window> always_on_top_window_;
  scoped_ptr<aura::Window> system_modal_window_;
  aura::Window* transient_child_;
  scoped_ptr<aura::Window> transient_parent_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DragWindowResizerTest);
};

// Verifies a window can be moved from the primary display to another.
TEST_F(DragWindowResizerTest, WindowDragWithMultiDisplays) {
  // The secondary display is logically on the right, but on the system (e.g. X)
  // layer, it's below the primary one. See UpdateDisplay() in ash_test_base.cc.
  UpdateDisplay("800x600,800x600");
  shelf_layout_manager()->LayoutShelf();
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             Shell::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  {
    // Grab (0, 0) of the window.
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    // Drag the pointer to the right. Once it reaches the right edge of the
    // primary display, it warps to the secondary.
    resizer->Drag(CalculateDragPoint(*resizer, 800, 10), 0);
    resizer->CompleteDrag(0);
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
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 790, 10), 0);
    resizer->CompleteDrag(0);
    // Since the pointer is still on the primary root window, the parent should
    // not be changed.
    EXPECT_EQ(root_windows[0], window_->GetRootWindow());
    EXPECT_EQ("790,10 50x60", window_->bounds().ToString());
  }

  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             Shell::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  {
    // Grab the top-right edge of the window and move the pointer to (0, 10)
    // in the secondary root window's coordinates.
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(49, 0), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 751, 10), ui::EF_CONTROL_DOWN);
    resizer->CompleteDrag(0);
    // Since the pointer is on the secondary, the parent should be changed
    // even though only small fraction of the window is within the secondary
    // root window's bounds.
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    EXPECT_EQ("-49,10 50x60", window_->bounds().ToString());
  }
}

// Verifies a window can be moved from the secondary display to primary.
TEST_F(DragWindowResizerTest, WindowDragWithMultiDisplaysRightToLeft) {
  UpdateDisplay("800x600,800x600");
  shelf_layout_manager()->LayoutShelf();
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  window_->SetBoundsInScreen(
      gfx::Rect(800, 00, 50, 60),
      Shell::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
  EXPECT_EQ(root_windows[1], window_->GetRootWindow());
  {
    // Grab (0, 0) of the window.
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    // Move the mouse near the right edge, (798, 0), of the primary display.
    resizer->Drag(CalculateDragPoint(*resizer, -2, 0), ui::EF_CONTROL_DOWN);
    resizer->CompleteDrag(0);
    EXPECT_EQ(root_windows[0], window_->GetRootWindow());
    EXPECT_EQ("798,0 50x60", window_->bounds().ToString());
  }
}

// Verifies the drag window is shown correctly.
TEST_F(DragWindowResizerTest, DragWindowController) {
  UpdateDisplay("800x600,800x600");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             Shell::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  {
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    EXPECT_FALSE(resizer->drag_window_controller_.get());

    // The pointer is inside the primary root. The drag window controller
    // should be NULL.
    resizer->Drag(CalculateDragPoint(*resizer, 10, 10), 0);
    EXPECT_FALSE(resizer->drag_window_controller_.get());

    // The window spans both root windows.
    resizer->Drag(CalculateDragPoint(*resizer, 798, 10), 0);
    DragWindowController* controller =
        resizer->drag_window_controller_.get();
    ASSERT_TRUE(controller);

    ASSERT_TRUE(controller->drag_widget_);
    ui::Layer* drag_layer =
        controller->drag_widget_->GetNativeWindow()->layer();
    ASSERT_TRUE(drag_layer);
    // Check if |resizer->layer_| is properly set to the drag widget.
    const std::vector<ui::Layer*>& layers = drag_layer->children();
    EXPECT_FALSE(layers.empty());
    EXPECT_EQ(controller->layer_, layers.back());

    // |window_| should be opaque since the pointer is still on the primary
    // root window. The drag window should be semi-transparent.
    EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
    ASSERT_TRUE(controller->drag_widget_);
    EXPECT_GT(1.0f, drag_layer->opacity());

    // Enter the pointer to the secondary display.
    resizer->Drag(CalculateDragPoint(*resizer, 800, 10), 0);
    controller = resizer->drag_window_controller_.get();
    ASSERT_TRUE(controller);
    // |window_| should be transparent, and the drag window should be opaque.
    EXPECT_GT(1.0f, window_->layer()->opacity());
    EXPECT_FLOAT_EQ(1.0f, drag_layer->opacity());

    resizer->CompleteDrag(0);
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  }

  // Do the same test with RevertDrag().
  window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                             Shell::GetScreen()->GetPrimaryDisplay());
  EXPECT_EQ(root_windows[0], window_->GetRootWindow());
  EXPECT_FLOAT_EQ(1.0f, window_->layer()->opacity());
  {
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    EXPECT_FALSE(resizer->drag_window_controller_.get());

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
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    // While dragging a window, warp should be allowed.
    EXPECT_EQ(MouseCursorEventFilter::WARP_DRAG,
              event_filter->mouse_warp_mode_);
    resizer->CompleteDrag(0);
  }
  EXPECT_EQ(MouseCursorEventFilter::WARP_ALWAYS,
            event_filter->mouse_warp_mode_);

  {
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    EXPECT_EQ(MouseCursorEventFilter::WARP_DRAG,
              event_filter->mouse_warp_mode_);
    resizer->RevertDrag();
  }
  EXPECT_EQ(MouseCursorEventFilter::WARP_ALWAYS,
            event_filter->mouse_warp_mode_);

  {
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTRIGHT));
    // While resizing a window, warp should NOT be allowed.
    EXPECT_EQ(MouseCursorEventFilter::WARP_NONE,
              event_filter->mouse_warp_mode_);
    resizer->CompleteDrag(0);
  }
  EXPECT_EQ(MouseCursorEventFilter::WARP_ALWAYS,
            event_filter->mouse_warp_mode_);

  {
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
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
  // The secondary display is logically on the right, but on the system (e.g. X)
  // layer, it's below the primary one. See UpdateDisplay() in ash_test_base.cc.
  UpdateDisplay("400x400,800x800*2");
  shelf_layout_manager()->LayoutShelf();
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  test::CursorManagerTestApi cursor_test_api(
      Shell::GetInstance()->cursor_manager());
  MouseCursorEventFilter* event_filter =
      Shell::GetInstance()->mouse_cursor_filter();
  // Move window from the root window with 1.0 device scale factor to the root
  // window with 2.0 device scale factor.
  {
    window_->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                               Shell::GetScreen()->GetPrimaryDisplay());
    EXPECT_EQ(root_windows[0], window_->GetRootWindow());
    // Grab (0, 0) of the window.
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    EXPECT_EQ(1.0f, cursor_test_api.GetDeviceScaleFactor());
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    event_filter->WarpMouseCursorIfNecessary(root_windows[0],
                                             gfx::Point(399, 200));
    EXPECT_EQ(2.0f, cursor_test_api.GetDeviceScaleFactor());
    resizer->CompleteDrag(0);
    EXPECT_EQ(2.0f, cursor_test_api.GetDeviceScaleFactor());
  }

  // Move window from the root window with 2.0 device scale factor to the root
  // window with 1.0 device scale factor.
  {
    window_->SetBoundsInScreen(
        gfx::Rect(600, 0, 50, 60),
        Shell::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
    EXPECT_EQ(root_windows[1], window_->GetRootWindow());
    // Grab (0, 0) of the window.
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window_.get(), gfx::Point(), HTCAPTION));
    EXPECT_EQ(2.0f, cursor_test_api.GetDeviceScaleFactor());
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, -200, 200), 0);
    event_filter->WarpMouseCursorIfNecessary(root_windows[1],
                                             gfx::Point(400, 200));
    EXPECT_EQ(1.0f, cursor_test_api.GetDeviceScaleFactor());
    resizer->CompleteDrag(0);
    EXPECT_EQ(1.0f, cursor_test_api.GetDeviceScaleFactor());
  }
}

// Verifies several kinds of windows can be moved across displays.
TEST_F(DragWindowResizerTest, MoveWindowAcrossDisplays) {
  // The secondary display is logically on the right, but on the system (e.g. X)
  // layer, it's below the primary one. See UpdateDisplay() in ash_test_base.cc.
  UpdateDisplay("400x400,400x400");
  shelf_layout_manager()->LayoutShelf();

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());
  MouseCursorEventFilter* event_filter =
      Shell::GetInstance()->mouse_cursor_filter();

  // Normal window can be moved across display.
  {
    aura::Window* window = window_.get();
    window->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                              Shell::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_TRUE(event_filter->WarpMouseCursorIfNecessary(root_windows[0],
                                                         gfx::Point(399, 200)));
    resizer->CompleteDrag(0);
  }

  // Always on top window can be moved across display.
  {
    aura::Window* window = always_on_top_window_.get();
    window->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                              Shell::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_TRUE(event_filter->WarpMouseCursorIfNecessary(root_windows[0],
                                                         gfx::Point(399, 200)));
    resizer->CompleteDrag(0);
  }

  // System modal window can be moved across display.
  {
    aura::Window* window = system_modal_window_.get();
    window->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                              Shell::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_TRUE(event_filter->WarpMouseCursorIfNecessary(root_windows[0],
                                                         gfx::Point(399, 200)));
    resizer->CompleteDrag(0);
  }

  // Transient window cannot be moved across display.
  {
    aura::Window* window = transient_child_;
    window->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                              Shell::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_FALSE(event_filter->WarpMouseCursorIfNecessary(
        root_windows[0],
        gfx::Point(399, 200)));
    resizer->CompleteDrag(0);
  }

  // The parent of transient window can be moved across display.
  {
    aura::Window* window = transient_parent_.get();
    window->SetBoundsInScreen(gfx::Rect(0, 0, 50, 60),
                              Shell::GetScreen()->GetPrimaryDisplay());
    // Grab (0, 0) of the window.
    scoped_ptr<DragWindowResizer> resizer(CreateDragWindowResizer(
        window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 399, 200), 0);
    EXPECT_TRUE(event_filter->WarpMouseCursorIfNecessary(root_windows[0],
                                                         gfx::Point(399, 200)));
    resizer->CompleteDrag(0);
  }
}

}  // namespace internal
}  // namespace ash
