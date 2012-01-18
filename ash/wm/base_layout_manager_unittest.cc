// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/base_layout_manager.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/base/ui_base_types.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

class BaseLayoutManagerTest : public aura::test::AuraTestBase {
 public:
  BaseLayoutManagerTest() : layout_manager_(NULL) {}
  virtual ~BaseLayoutManagerTest() {}

  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    aura::RootWindow::GetInstance()->screen()->set_work_area_insets(
        gfx::Insets(1, 2, 3, 4));
    aura::RootWindow::GetInstance()->SetHostSize(gfx::Size(800, 600));
    container_.reset(new aura::Window(NULL));
    container_->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
    container_->SetBounds(gfx::Rect(0, 0, 500, 500));
    layout_manager_ = new internal::BaseLayoutManager();
    container_->SetLayoutManager(layout_manager_);
  }

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    return aura::test::CreateTestWindowWithBounds(bounds, container_.get());
  }

 private:
  // Owned by |container_|.
  internal::BaseLayoutManager* layout_manager_;

  scoped_ptr<aura::Window> container_;

  DISALLOW_COPY_AND_ASSIGN(BaseLayoutManagerTest);
};

}  // namespace

// Tests normal->maximize->normal.
TEST_F(BaseLayoutManagerTest, Maximize) {
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
TEST_F(BaseLayoutManagerTest, MaximizeRootWindowResize) {
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
TEST_F(BaseLayoutManagerTest, Fullscreen) {
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
TEST_F(BaseLayoutManagerTest, FullscreenRootWindowResize) {
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
TEST_F(BaseLayoutManagerTest, RootWindowResizeShrinksWindows) {
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

}  // namespace ash
