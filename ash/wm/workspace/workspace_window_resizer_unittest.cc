// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/string_number_conversions.h"
#include "ui/aura/root_window.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/hit_test.h"

namespace ash {
namespace internal {
namespace {

const int kRootHeight = 600;

// A simple window delegate that returns the specified min size.
class TestWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  TestWindowDelegate() {
  }
  virtual ~TestWindowDelegate() {}

  void set_min_size(const gfx::Size& size) {
    min_size_ = size;
  }

 private:
  // Overridden from aura::Test::TestWindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    return min_size_;
  }

  gfx::Size min_size_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegate);
};

class WorkspaceWindowResizerTest : public test::AshTestBase {
 public:
  WorkspaceWindowResizerTest() : window_(NULL) {}
  virtual ~WorkspaceWindowResizerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    aura::RootWindow* root = Shell::GetInstance()->GetRootWindow();
    root->SetBounds(gfx::Rect(0, 0, 800, kRootHeight));
    gfx::Rect root_bounds(root->bounds());
    EXPECT_EQ(kRootHeight, root_bounds.height());
    root->SetScreenWorkAreaInsets(gfx::Insets());
    window_.reset(new aura::Window(&delegate_));
    window_->Init(ui::Layer::LAYER_NOT_DRAWN);
    window_->SetParent(Shell::GetInstance()->GetRootWindow());

    window2_.reset(new aura::Window(&delegate2_));
    window2_->Init(ui::Layer::LAYER_NOT_DRAWN);
    window2_->SetParent(Shell::GetInstance()->GetRootWindow());

    window3_.reset(new aura::Window(&delegate3_));
    window3_->Init(ui::Layer::LAYER_NOT_DRAWN);
    window3_->SetParent(Shell::GetInstance()->GetRootWindow());
  }

  virtual void TearDown() OVERRIDE {
    window_.reset();
    window2_.reset();
    window3_.reset();
    AshTestBase::TearDown();
  }

 protected:
  gfx::Point CalculateDragPoint(const WorkspaceWindowResizer& resizer,
                                int delta_x,
                                int delta_y) const {
    gfx::Point location = resizer.initial_location_in_parent();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    return location;
  }

  TestWindowDelegate delegate_;
  TestWindowDelegate delegate2_;
  TestWindowDelegate delegate3_;
  scoped_ptr<aura::Window> window_;
  scoped_ptr<aura::Window> window2_;
  scoped_ptr<aura::Window> window3_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkspaceWindowResizerTest);
};

// Assertions around making sure dragging shrinks when appropriate.
TEST_F(WorkspaceWindowResizerTest, ShrinkOnDrag) {
  int initial_y = 300;
  window_->SetBounds(gfx::Rect(0, initial_y, 400, 296));

  // Drag down past the bottom of the screen, height should stop when it hits
  // the bottom.
  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTBOTTOM, 0));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 600));
    EXPECT_EQ(kRootHeight - initial_y, window_->bounds().height());

    // Drag up 10 and make sure height is the same.
    resizer->Drag(CalculateDragPoint(*resizer, 0, 590));
    EXPECT_EQ(kRootHeight - initial_y, window_->bounds().height());
  }

  {
    // Move the window down 10 pixels, the height should change.
    int initial_height = window_->bounds().height();
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTCAPTION, 0));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 10));
    EXPECT_EQ(initial_height - 10, window_->bounds().height());

    // Move up 10, height should grow.
    resizer->Drag(CalculateDragPoint(*resizer, 0, 0));
    EXPECT_EQ(initial_height, window_->bounds().height());

    // Move up another 10, height shouldn't change.
    resizer->Drag(CalculateDragPoint(*resizer, 0, -10));
    EXPECT_EQ(initial_height, window_->bounds().height());
  }
}

// More assertions around making sure dragging shrinks when appropriate.
TEST_F(WorkspaceWindowResizerTest, ShrinkOnDrag2) {
  window_->SetBounds(gfx::Rect(0, 300, 400, 300));

  // Drag down past the bottom of the screen, height should stop when it hits
  // the bottom.
  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTCAPTION, 0));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 200));
    EXPECT_EQ(500, window_->bounds().y());
    EXPECT_EQ(100, window_->bounds().height());
    // End and start a new drag session.
  }

  {
    // Drag up 400.
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTCAPTION, 0));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, -400));
    EXPECT_EQ(100, window_->bounds().y());
    EXPECT_EQ(300, window_->bounds().height());
  }
}

// Moves enough to shrink, then moves up twice to expose more than was initially
// exposed.
TEST_F(WorkspaceWindowResizerTest, ShrinkMoveThanMoveUp) {
  window_->SetBounds(gfx::Rect(0, 300, 400, 300));

  // Drag down past the bottom of the screen, height should stop when it hits
  // the bottom.
  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTCAPTION, 0));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 200));
    EXPECT_EQ(500, window_->bounds().y());
    EXPECT_EQ(100, window_->bounds().height());
    // End and start a new drag session.
  }

  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTCAPTION, 0));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, -400));
    resizer->Drag(CalculateDragPoint(*resizer, 0, -450));
    EXPECT_EQ(50, window_->bounds().y());
    EXPECT_EQ(300, window_->bounds().height());
  }
}

// Makes sure shrinking honors the grid appropriately.
TEST_F(WorkspaceWindowResizerTest, ShrinkWithGrid) {
  window_->SetBounds(gfx::Rect(0, 300, 400, 296));

  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTCAPTION, 5));
  ASSERT_TRUE(resizer.get());
  // Drag down 8 pixels.
  resizer->Drag(CalculateDragPoint(*resizer, 0, 8));
  resizer->CompleteDrag();
  EXPECT_EQ(310, window_->bounds().y());
  EXPECT_EQ(kRootHeight - 310, window_->bounds().height());
}

// Makes sure once a window has been shrunk it can grow bigger than obscured
// height
TEST_F(WorkspaceWindowResizerTest, ShrinkThanGrow) {
  int initial_y = 400;
  int initial_height = 150;
  window_->SetBounds(gfx::Rect(0, initial_y, 400, initial_height));

  // Most past the bottom of the screen, height should stop when it hits the
  // bottom.
  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTCAPTION, 0));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 150));
    EXPECT_EQ(550, window_->bounds().y());
    EXPECT_EQ(50, window_->bounds().height());
  }

  // Resize the window 500 pixels up.
  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTTOP, 0));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, -500));
    EXPECT_EQ(50, window_->bounds().y());
    EXPECT_EQ(550, window_->bounds().height());
  }
}

// Makes sure once a window has been shrunk it can grow bigger than obscured
// height
TEST_F(WorkspaceWindowResizerTest, DontRememberAfterMove) {
  window_->SetBounds(gfx::Rect(0, 300, 400, 300));

  // Most past the bottom of the screen, height should stop when it hits the
  // bottom.
  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTCAPTION, 0));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 150));
    EXPECT_EQ(450, window_->bounds().y());
    EXPECT_EQ(150, window_->bounds().height());
    resizer->Drag(CalculateDragPoint(*resizer, 0, -150));
    EXPECT_EQ(150, window_->bounds().y());
    EXPECT_EQ(300, window_->bounds().height());
  }

  // Resize it slightly.
  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTBOTTOM, 0));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, -100));
    EXPECT_EQ(150, window_->bounds().y());
    EXPECT_EQ(200, window_->bounds().height());
  }

  {
    // Move it down then back up.
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTCAPTION, 0));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 400));
    EXPECT_EQ(550, window_->bounds().y());
    EXPECT_EQ(50, window_->bounds().height());

    resizer->Drag(CalculateDragPoint(*resizer, 0, 0));
    EXPECT_EQ(150, window_->bounds().y());
    EXPECT_EQ(200, window_->bounds().height());
  }
}

// Makes sure we honor the min size.
TEST_F(WorkspaceWindowResizerTest, HonorMin) {
  delegate_.set_min_size(gfx::Size(50, 100));
  window_->SetBounds(gfx::Rect(0, 300, 400, 300));

  // Most past the bottom of the screen, height should stop when it hits the
  // bottom.
  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTCAPTION, 0));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 350));
    EXPECT_EQ(500, window_->bounds().y());
    EXPECT_EQ(100, window_->bounds().height());

    resizer->Drag(CalculateDragPoint(*resizer, 0, 300));
    EXPECT_EQ(500, window_->bounds().y());
    EXPECT_EQ(100, window_->bounds().height());

    resizer->Drag(CalculateDragPoint(*resizer, 0, 250));
    EXPECT_EQ(500, window_->bounds().y());
    EXPECT_EQ(100, window_->bounds().height());

    resizer->Drag(CalculateDragPoint(*resizer, 0, 100));
    EXPECT_EQ(400, window_->bounds().y());
    EXPECT_EQ(200, window_->bounds().height());

    resizer->Drag(CalculateDragPoint(*resizer, 0, -100));
    EXPECT_EQ(200, window_->bounds().y());
    EXPECT_EQ(300, window_->bounds().height());
  }
}

}  // namespace
}  // namespace test
}  // namespace aura
