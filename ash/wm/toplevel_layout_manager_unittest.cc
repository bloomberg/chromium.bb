// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/toplevel_layout_manager.h"

#include "ash/wm/shelf_layout_manager.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/base/ui_base_types.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class ToplevelLayoutManagerTest : public aura::test::AuraTestBase {
 public:
  ToplevelLayoutManagerTest() : layout_manager_(NULL) {}
  virtual ~ToplevelLayoutManagerTest() {}

  internal::ToplevelLayoutManager* layout_manager() {
    return layout_manager_;
  }

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    aura::RootWindow::GetInstance()->screen()->set_work_area_insets(
        gfx::Insets(1, 2, 3, 4));
    aura::RootWindow::GetInstance()->SetHostSize(gfx::Size(800, 600));
    container_.reset(new aura::Window(NULL));
    container_->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
    container_->SetBounds(gfx::Rect(0, 0, 500, 500));
    layout_manager_ = new internal::ToplevelLayoutManager();
    container_->SetLayoutManager(layout_manager_);
  }

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    aura::Window* window = new aura::Window(NULL);
    window->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
    window->SetBounds(bounds);
    window->Show();
    window->SetParent(container_.get());
    return window;
  }

  // Returns widget owned by its parent, so doesn't need scoped_ptr<>.
  views::Widget* CreateTestWidget() {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
    params.bounds = gfx::Rect(11, 22, 33, 44);
    widget->Init(params);
    widget->Show();
    return widget;
  }

 private:
  // Owned by |container_|.
  internal::ToplevelLayoutManager* layout_manager_;

  scoped_ptr<aura::Window> container_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelLayoutManagerTest);
};

}  // namespace

// Tests normal->maximize->normal.
TEST_F(ToplevelLayoutManagerTest, Maximize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  // Maximized window fills the work area, not the whole monitor.
  EXPECT_EQ(gfx::Screen::GetMonitorWorkAreaNearestWindow(window.get()),
            window->bounds());
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(bounds, window->bounds());
}

// Tests maximized window size during root window resize.
TEST_F(ToplevelLayoutManagerTest, MaximizeRootWindowResize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(gfx::Screen::GetMonitorWorkAreaNearestWindow(window.get()),
            window->bounds());
  // Enlarge the root window.  We should still match the work area size.
  aura::RootWindow::GetInstance()->SetHostSize(gfx::Size(800, 600));
  EXPECT_EQ(gfx::Screen::GetMonitorWorkAreaNearestWindow(window.get()),
            window->bounds());
}

// Tests normal->fullscreen->normal.
TEST_F(ToplevelLayoutManagerTest, Fullscreen) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  window->SetIntProperty(aura::client::kShowStateKey,
                         ui::SHOW_STATE_FULLSCREEN);
  // Fullscreen window fills the whole monitor.
  EXPECT_EQ(gfx::Screen::GetMonitorAreaNearestWindow(window.get()),
            window->bounds());
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(bounds, window->bounds());
}

// Tests fullscreen window size during root window resize.
TEST_F(ToplevelLayoutManagerTest, FullscreenRootWindowResize) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  // Fullscreen window fills the whole monitor.
  window->SetIntProperty(aura::client::kShowStateKey,
                         ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(gfx::Screen::GetMonitorAreaNearestWindow(window.get()),
            window->bounds());
  // Enlarge the root window.  We should still match the monitor size.
  aura::RootWindow::GetInstance()->SetHostSize(gfx::Size(800, 600));
  EXPECT_EQ(gfx::Screen::GetMonitorAreaNearestWindow(window.get()),
            window->bounds());
}

// Tests that when the screen gets smaller the windows aren't bigger than
// the screen.
TEST_F(ToplevelLayoutManagerTest, RootWindowResizeShrinksWindows) {
  scoped_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(10, 20, 500, 400)));
  gfx::Rect work_area = gfx::Screen::GetMonitorAreaNearestWindow(window.get());
  // Invariant: Window is smaller than work area.
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Make the root window narrower than our window.
  aura::RootWindow::GetInstance()->SetHostSize(gfx::Size(300, 400));
  work_area = gfx::Screen::GetMonitorAreaNearestWindow(window.get());
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Make the root window shorter than our window.
  aura::RootWindow::GetInstance()->SetHostSize(gfx::Size(300, 200));
  work_area = gfx::Screen::GetMonitorAreaNearestWindow(window.get());
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Enlarging the root window does not change the window bounds.
  gfx::Rect old_bounds = window->bounds();
  aura::RootWindow::GetInstance()->SetHostSize(gfx::Size(800, 600));
  EXPECT_EQ(old_bounds.width(), window->bounds().width());
  EXPECT_EQ(old_bounds.height(), window->bounds().height());
}

// Test that window stays on screen despite attempts to move it off.
TEST_F(ToplevelLayoutManagerTest, WindowStaysOnScreen) {
  scoped_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(10, 20, 300, 200)));
  gfx::Rect work_area = gfx::Screen::GetMonitorAreaNearestWindow(window.get());
  // Invariant: The top edge of the window is inside the work area and at least
  // one pixel is visible horizontally.
  // and horizontal dimensions.
  EXPECT_GE(window->bounds().y(), 0);
  EXPECT_LT(window->bounds().y(), work_area.bottom());
  EXPECT_GE(window->bounds().right(), 0);
  EXPECT_LT(window->bounds().x(), work_area.right());

  // Try to move window above the top of screen.
  window->SetBounds(gfx::Rect(10, -1, 300, 200));
  EXPECT_GE(window->bounds().y(), 0);

  // Try to move window below the bottom.
  window->SetBounds(gfx::Rect(10, work_area.bottom(), 300, 200));
  EXPECT_LT(window->bounds().y(), work_area.bottom());

  // Put the screen near the right edge, then shrink the host window.
  window->SetBounds(gfx::Rect(work_area.right() - 15, 20, 300, 200));
  aura::RootWindow::GetInstance()->SetHostSize(gfx::Size(350, 250));
  EXPECT_GE(window->bounds().y(), 0);
  EXPECT_LT(window->bounds().y(), work_area.bottom());
  EXPECT_GE(window->bounds().right(), 0);
  EXPECT_LT(window->bounds().x(), work_area.right());
}

// Tests status area visibility during window maximize and fullscreen.
TEST_F(ToplevelLayoutManagerTest, StatusAreaVisibility) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  views::Widget* widget = CreateTestWidget();
  layout_manager()->set_status_area_widget(widget);
  EXPECT_TRUE(widget->IsVisible());
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_TRUE(widget->IsVisible());
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_TRUE(widget->IsVisible());
  window->SetIntProperty(aura::client::kShowStateKey,
                         ui::SHOW_STATE_FULLSCREEN);
  EXPECT_FALSE(widget->IsVisible());
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_TRUE(widget->IsVisible());
}

// Tests shelf visibility during window maximize and fullscreen.
TEST_F(ToplevelLayoutManagerTest, ShelfVisibility) {
  gfx::Rect bounds(100, 100, 200, 200);
  scoped_ptr<aura::Window> window(CreateTestWindow(bounds));
  views::Widget* launcher = CreateTestWidget();
  views::Widget* status = CreateTestWidget();
  scoped_ptr<internal::ShelfLayoutManager> shelf_layout_manager(
      new internal::ShelfLayoutManager(launcher, status));
  layout_manager()->set_shelf(shelf_layout_manager.get());
  EXPECT_TRUE(shelf_layout_manager.get()->visible());
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_TRUE(shelf_layout_manager.get()->visible());
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_TRUE(shelf_layout_manager.get()->visible());
  window->SetIntProperty(aura::client::kShowStateKey,
                         ui::SHOW_STATE_FULLSCREEN);
  EXPECT_FALSE(shelf_layout_manager.get()->visible());
  window->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_TRUE(shelf_layout_manager.get()->visible());
  // Window depends on layout manager for cleanup.
  window.reset();
  // shelf_layout_manager is observing these animations so clean them up first.
  launcher->GetNativeView()->layer()->GetAnimator()->StopAnimating();
  status->GetNativeView()->layer()->GetAnimator()->StopAnimating();
  shelf_layout_manager.reset();
}

}  // namespace ash
