// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/default_window_resizer.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"

namespace ash {

class DefaultWindowResizerTest : public AshTestBase {
 public:
  DefaultWindowResizerTest() = default;
  ~DefaultWindowResizerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    UpdateDisplay("1000x1000");

    delegate_.set_minimum_size(gfx::Size(10, 10));
    delegate_.set_maximum_size(gfx::Size(500, 500));
    aspect_ratio_window_ = std::make_unique<aura::Window>(&delegate_);
    aspect_ratio_window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
    aspect_ratio_window_->Init(ui::LAYER_NOT_DRAWN);
    ParentWindowInPrimaryRootWindow(aspect_ratio_window_.get());
  }

  void TearDown() override {
    aspect_ratio_window_.reset();
    AshTestBase::TearDown();
  }

 protected:
  static WindowResizer* CreateDefaultWindowResizer(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component) {
    return CreateWindowResizer(window, point_in_parent, window_component,
                               ::wm::WINDOW_MOVE_SOURCE_MOUSE)
        .release();
  }

  aura::test::TestWindowDelegate delegate_;
  std::unique_ptr<aura::Window> aspect_ratio_window_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultWindowResizerTest);
};

// Tests window dragging with a square aspect ratio.
TEST_F(DefaultWindowResizerTest, WindowResizeWithAspectRatioSquare) {
  aspect_ratio_window_->SetProperty(aura::client::kAspectRatio,
                                    new gfx::SizeF(1.0, 1.0));

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(1U, root_windows.size());
  EXPECT_EQ(root_windows[0], aspect_ratio_window_->GetRootWindow());

  aspect_ratio_window_->SetBoundsInScreen(
      gfx::Rect(200, 200, 200, 200),
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[0]));
  EXPECT_EQ("200,200 200x200", aspect_ratio_window_->bounds().ToString());

  std::unique_ptr<WindowResizer> resizer(CreateDefaultWindowResizer(
      aspect_ratio_window_.get(), gfx::Point(), HTTOPLEFT));
  ASSERT_TRUE(resizer.get());

  // Move the mouse near the top left edge.
  resizer->Drag(gfx::Point(50, 50), 0);
  resizer->CompleteDrag();
  EXPECT_EQ(root_windows[0], aspect_ratio_window_->GetRootWindow());
  EXPECT_EQ("250,250 150x150", aspect_ratio_window_->bounds().ToString());
}

// Tests window dragging with a horizontal orientation aspect ratio.
TEST_F(DefaultWindowResizerTest, WindowResizeWithAspectRatioHorizontal) {
  aspect_ratio_window_->SetProperty(aura::client::kAspectRatio,
                                    new gfx::SizeF(2.0, 1.0));

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(1U, root_windows.size());
  EXPECT_EQ(root_windows[0], aspect_ratio_window_->GetRootWindow());

  aspect_ratio_window_->SetBoundsInScreen(
      gfx::Rect(200, 200, 400, 200),
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[0]));
  EXPECT_EQ("200,200 400x200", aspect_ratio_window_->bounds().ToString());

  std::unique_ptr<WindowResizer> resizer(CreateDefaultWindowResizer(
      aspect_ratio_window_.get(), gfx::Point(), HTBOTTOMRIGHT));
  ASSERT_TRUE(resizer.get());

  // Move the mouse near the top left edge.
  resizer->Drag(gfx::Point(50, 50), 0);
  resizer->CompleteDrag();
  EXPECT_EQ(root_windows[0], aspect_ratio_window_->GetRootWindow());
  EXPECT_EQ("200,200 500x250", aspect_ratio_window_->bounds().ToString());
}

// Tests window dragging with a vertical orientation aspect ratio.
TEST_F(DefaultWindowResizerTest, WindowResizeWithAspectRatioVertical) {
  aspect_ratio_window_->SetProperty(aura::client::kAspectRatio,
                                    new gfx::SizeF(1.0, 2.0));

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(1U, root_windows.size());
  EXPECT_EQ(root_windows[0], aspect_ratio_window_->GetRootWindow());

  aspect_ratio_window_->SetBoundsInScreen(
      gfx::Rect(200, 200, 200, 400),
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[0]));
  EXPECT_EQ("200,200 200x400", aspect_ratio_window_->bounds().ToString());

  std::unique_ptr<WindowResizer> resizer(CreateDefaultWindowResizer(
      aspect_ratio_window_.get(), gfx::Point(), HTBOTTOM));
  ASSERT_TRUE(resizer.get());

  // Move the mouse near the top left edge.
  resizer->Drag(gfx::Point(50, 50), 0);
  resizer->CompleteDrag();
  EXPECT_EQ(root_windows[0], aspect_ratio_window_->GetRootWindow());
  EXPECT_EQ("200,200 225x450", aspect_ratio_window_->bounds().ToString());
}

}  // namespace ash
