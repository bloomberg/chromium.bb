// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/property_util.h"
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

  std::vector<aura::Window*> empty_windows() const {
    return std::vector<aura::Window*>();
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
        window_.get(), gfx::Point(), HTBOTTOM, 0, empty_windows()));
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
        window_.get(), gfx::Point(), HTCAPTION, 0, empty_windows()));
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
        window_.get(), gfx::Point(), HTCAPTION, 0, empty_windows()));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 200));
    EXPECT_EQ(500, window_->bounds().y());
    EXPECT_EQ(100, window_->bounds().height());
    // End and start a new drag session.
  }

  {
    // Drag up 400.
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTCAPTION, 0, empty_windows()));
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
        window_.get(), gfx::Point(), HTCAPTION, 0, empty_windows()));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 200));
    EXPECT_EQ(500, window_->bounds().y());
    EXPECT_EQ(100, window_->bounds().height());
    // End and start a new drag session.
  }

  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTCAPTION, 0, empty_windows()));
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
      window_.get(), gfx::Point(), HTCAPTION, 5, empty_windows()));
  ASSERT_TRUE(resizer.get());
  // Drag down 8 pixels.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 8));
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
        window_.get(), gfx::Point(), HTCAPTION, 0, empty_windows()));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 150));
    EXPECT_EQ(550, window_->bounds().y());
    EXPECT_EQ(50, window_->bounds().height());
  }

  // Resize the window 500 pixels up.
  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTTOP, 0, empty_windows()));
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
        window_.get(), gfx::Point(), HTCAPTION, 0, empty_windows()));
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
        window_.get(), gfx::Point(), HTBOTTOM, 0, empty_windows()));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, -100));
    EXPECT_EQ(150, window_->bounds().y());
    EXPECT_EQ(200, window_->bounds().height());
  }

  {
    // Move it down then back up.
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTCAPTION, 0, empty_windows()));
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
        window_.get(), gfx::Point(), HTCAPTION, 0, empty_windows()));
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

// Assertions around attached window resize dragging from the right with 2
// windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_2) {
  window_->SetBounds(gfx::Rect(0, 300, 400, 300));
  window2_->SetBounds(gfx::Rect(400, 200, 100, 200));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT, 0, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the right, which should expand w1 and push w2.
  resizer->Drag(CalculateDragPoint(*resizer, 100, 10));
  EXPECT_EQ("0,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("500,200 100x200", window2_->bounds().ToString());

  // Push off the screen, w2 should be resized to its min.
  delegate2_.set_min_size(gfx::Size(20, 20));
  resizer->Drag(CalculateDragPoint(*resizer, 800, 20));
  EXPECT_EQ("0,300 780x300", window_->bounds().ToString());
  EXPECT_EQ("780,200 20x200", window2_->bounds().ToString());

  // Move back to 100 and verify w2 gets its original size.
  resizer->Drag(CalculateDragPoint(*resizer, 100, 10));
  EXPECT_EQ("0,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("500,200 100x200", window2_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->Drag(CalculateDragPoint(*resizer, 800, 20));
  resizer->RevertDrag();
  EXPECT_EQ("0,300 400x300", window_->bounds().ToString());
  EXPECT_EQ("400,200 100x200", window2_->bounds().ToString());
}

// Makes sure we remember the size of an attached window across drags when
// compressing.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_2_REMEMBER) {
  window_->SetBounds(gfx::Rect(0, 300, 400, 300));
  window2_->SetBounds(gfx::Rect(400, 200, 100, 200));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());

  {
    delegate2_.set_min_size(gfx::Size(20, 20));
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTRIGHT, 0, windows));
    ASSERT_TRUE(resizer.get());
    // Resize enough to slightly compress w2.
    resizer->Drag(CalculateDragPoint(*resizer, 350, 10));
    EXPECT_EQ("0,300 750x300", window_->bounds().ToString());
    EXPECT_EQ("750,200 50x200", window2_->bounds().ToString());

    // Compress w2 a bit more.
    resizer->Drag(CalculateDragPoint(*resizer, 400, 10));
    EXPECT_EQ("0,300 780x300", window_->bounds().ToString());
    EXPECT_EQ("780,200 20x200", window2_->bounds().ToString());

    resizer->CompleteDrag();
  }

  // Restart drag and drag back 200, making sure window2 goes back to 200.
  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTRIGHT, 0, windows));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, -200, 10));
    EXPECT_EQ("0,300 580x300", window_->bounds().ToString());
    EXPECT_EQ("580,200 100x200", window2_->bounds().ToString());
  }
}

// Assertions around attached window resize dragging from the right with 3
// windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_3) {
  window_->SetBounds(gfx::Rect( 100, 300, 200, 300));
  window2_->SetBounds(gfx::Rect(300, 300, 150, 200));
  window3_->SetBounds(gfx::Rect(450, 300, 100, 200));
  delegate2_.set_min_size(gfx::Size(52, 50));
  delegate3_.set_min_size(gfx::Size(38, 50));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT, 10, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the right, which should expand w1 and push w2 and w3.
  resizer->Drag(CalculateDragPoint(*resizer, 100, -10));
  EXPECT_EQ("100,300 300x300", window_->bounds().ToString());
  EXPECT_EQ("400,300 150x200", window2_->bounds().ToString());
  EXPECT_EQ("550,300 100x200", window3_->bounds().ToString());

  // Move it 296, which should now snap to grid and things should compress.
  resizer->Drag(CalculateDragPoint(*resizer, 296, -10));
  EXPECT_EQ("100,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("600,300 120x200", window2_->bounds().ToString());
  EXPECT_EQ("720,300 80x200", window3_->bounds().ToString());

  // Move it so much everything ends up at it's min.
  resizer->Drag(CalculateDragPoint(*resizer, 798, 50));
  EXPECT_EQ("100,300 600x300", window_->bounds().ToString());
  EXPECT_EQ("700,300 60x200", window2_->bounds().ToString());
  EXPECT_EQ("760,300 40x200", window3_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->RevertDrag();
  EXPECT_EQ("100,300 200x300", window_->bounds().ToString());
  EXPECT_EQ("300,300 150x200", window2_->bounds().ToString());
  EXPECT_EQ("450,300 100x200", window3_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_RememberWidth) {
  window_->SetBounds(gfx::Rect( 100, 300, 200, 300));
  window2_->SetBounds(gfx::Rect(300, 300, 150, 200));
  window3_->SetBounds(gfx::Rect(450, 300, 100, 200));
  delegate2_.set_min_size(gfx::Size(52, 50));
  delegate3_.set_min_size(gfx::Size(38, 50));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());

  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTRIGHT, 10, windows));
    ASSERT_TRUE(resizer.get());
    // Move it 100 to the right, which should expand w1 and push w2 and w3.
    resizer->Drag(CalculateDragPoint(*resizer, 100, -10));
    EXPECT_EQ("100,300 300x300", window_->bounds().ToString());
    EXPECT_EQ("400,300 150x200", window2_->bounds().ToString());
    EXPECT_EQ("550,300 100x200", window3_->bounds().ToString());

    // Move it so much everything ends up at it's min.
    resizer->Drag(CalculateDragPoint(*resizer, 798, 50));
    EXPECT_EQ("100,300 600x300", window_->bounds().ToString());
    EXPECT_EQ("700,300 60x200", window2_->bounds().ToString());
    EXPECT_EQ("760,300 40x200", window3_->bounds().ToString());
  }

  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTRIGHT, 10, windows));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, -100, 50));
    EXPECT_EQ("100,300 500x300", window_->bounds().ToString());
    EXPECT_EQ("600,300 120x200", window2_->bounds().ToString());
    EXPECT_EQ("720,300 80x200", window3_->bounds().ToString());

    // Move it 300 to the left.
    resizer->Drag(CalculateDragPoint(*resizer, -300, 50));
    EXPECT_EQ("100,300 300x300", window_->bounds().ToString());
    EXPECT_EQ("400,300 150x200", window2_->bounds().ToString());
    EXPECT_EQ("550,300 100x200", window3_->bounds().ToString());
  }

}

// Assertions around attached window resize dragging from the bottom with 2
// windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_2) {
  window_->SetBounds(gfx::Rect( 0,  50, 400, 200));
  window2_->SetBounds(gfx::Rect(0, 250, 200, 100));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTBOTTOM, 0, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the bottom, which should expand w1 and push w2.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 100));
  EXPECT_EQ("0,50 400x300", window_->bounds().ToString());
  EXPECT_EQ("0,350 200x100", window2_->bounds().ToString());

  // Push off the screen, w2 should be resized to its min.
  delegate2_.set_min_size(gfx::Size(20, 20));
  resizer->Drag(CalculateDragPoint(*resizer, 50, 820));
  EXPECT_EQ("0,50 400x530", window_->bounds().ToString());
  EXPECT_EQ("0,580 200x20", window2_->bounds().ToString());

  // Move back to 100 and verify w2 gets its original size.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 100));
  EXPECT_EQ("0,50 400x300", window_->bounds().ToString());
  EXPECT_EQ("0,350 200x100", window2_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->Drag(CalculateDragPoint(*resizer, 800, 20));
  resizer->RevertDrag();
  EXPECT_EQ("0,50 400x200", window_->bounds().ToString());
  EXPECT_EQ("0,250 200x100", window2_->bounds().ToString());
}

// Makes sure we remember the size of an attached window across drags when
// compressing.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_2_REMEMBER) {
  window_->SetBounds(gfx::Rect(  0,   0, 400, 300));
  window2_->SetBounds(gfx::Rect(40, 300, 100, 200));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());

  {
    delegate2_.set_min_size(gfx::Size(20, 20));
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTBOTTOM, 0, windows));
    ASSERT_TRUE(resizer.get());
    // Resize enough to slightly compress w2.
    resizer->Drag(CalculateDragPoint(*resizer, 10, 150));
    EXPECT_EQ("0,0 400x450", window_->bounds().ToString());
    EXPECT_EQ("40,450 100x150", window2_->bounds().ToString());

    // Compress w2 a bit more.
    resizer->Drag(CalculateDragPoint(*resizer, 5, 400));
    EXPECT_EQ("0,0 400x580", window_->bounds().ToString());
    EXPECT_EQ("40,580 100x20", window2_->bounds().ToString());

    resizer->CompleteDrag();
  }

  // Restart drag and drag back 200, making sure window2 goes back to 200.
  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTBOTTOM, 0, windows));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, -10, -200));
    EXPECT_EQ("0,0 400x380", window_->bounds().ToString());
    EXPECT_EQ("40,380 100x200", window2_->bounds().ToString());
  }
}

// Assertions around attached window resize dragging from the bottom with 3
// windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_3) {
  aura::RootWindow* root = Shell::GetInstance()->GetRootWindow();
  root->SetBounds(gfx::Rect(0, 0, 600, 800));
  root->SetScreenWorkAreaInsets(gfx::Insets());

  window_->SetBounds(gfx::Rect( 300, 100, 300, 200));
  window2_->SetBounds(gfx::Rect(300, 300, 200, 150));
  window3_->SetBounds(gfx::Rect(300, 450, 200, 100));
  delegate2_.set_min_size(gfx::Size(50, 52));
  delegate3_.set_min_size(gfx::Size(50, 38));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTBOTTOM, 10, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the right, which should expand w1 and push w2 and w3.
  resizer->Drag(CalculateDragPoint(*resizer, -10, 100));
  EXPECT_EQ("300,100 300x300", window_->bounds().ToString());
  EXPECT_EQ("300,400 200x150", window2_->bounds().ToString());
  EXPECT_EQ("300,550 200x100", window3_->bounds().ToString());

  // Move it 296, which should now snap to grid and things should compress.
  resizer->Drag(CalculateDragPoint(*resizer, -10, 296));
  EXPECT_EQ("300,100 300x500", window_->bounds().ToString());
  EXPECT_EQ("300,600 200x120", window2_->bounds().ToString());
  EXPECT_EQ("300,720 200x80", window3_->bounds().ToString());

  // Move it so much everything ends up at it's min.
  resizer->Drag(CalculateDragPoint(*resizer, 50, 798));
  EXPECT_EQ("300,100 300x600", window_->bounds().ToString());
  EXPECT_EQ("300,700 200x60", window2_->bounds().ToString());
  EXPECT_EQ("300,760 200x40", window3_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->RevertDrag();
  EXPECT_EQ("300,100 300x200", window_->bounds().ToString());
  EXPECT_EQ("300,300 200x150", window2_->bounds().ToString());
  EXPECT_EQ("300,450 200x100", window3_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_RememberHeight) {
  aura::RootWindow* root = Shell::GetInstance()->GetRootWindow();
  root->SetBounds(gfx::Rect(0, 0, 600, 800));
  root->SetScreenWorkAreaInsets(gfx::Insets());

  window_->SetBounds(gfx::Rect( 300, 100, 300, 200));
  window2_->SetBounds(gfx::Rect(300, 300, 200, 150));
  window3_->SetBounds(gfx::Rect(300, 450, 200, 100));
  delegate2_.set_min_size(gfx::Size(50, 52));
  delegate3_.set_min_size(gfx::Size(50, 38));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());

  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTBOTTOM, 10, windows));
    ASSERT_TRUE(resizer.get());
    // Move it 100 to the bottom, which should expand w1 and push w2 and w3.
    resizer->Drag(CalculateDragPoint(*resizer, -10, 100));
    EXPECT_EQ("300,100 300x300", window_->bounds().ToString());
    EXPECT_EQ("300,400 200x150", window2_->bounds().ToString());
    EXPECT_EQ("300,550 200x100", window3_->bounds().ToString());

    // Move it so much everything ends up at it's min.
    resizer->Drag(CalculateDragPoint(*resizer, 50, 798));
    EXPECT_EQ("300,100 300x600", window_->bounds().ToString());
    EXPECT_EQ("300,700 200x60", window2_->bounds().ToString());
    EXPECT_EQ("300,760 200x40", window3_->bounds().ToString());
  }

  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTBOTTOM, 10, windows));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 50, -100));
    EXPECT_EQ("300,100 300x500", window_->bounds().ToString());
    EXPECT_EQ("300,600 200x120", window2_->bounds().ToString());
    EXPECT_EQ("300,720 200x80", window3_->bounds().ToString());

    // Move it 300 up.
    resizer->Drag(CalculateDragPoint(*resizer, 50, -300));
    EXPECT_EQ("300,100 300x300", window_->bounds().ToString());
    EXPECT_EQ("300,400 200x150", window2_->bounds().ToString());
    EXPECT_EQ("300,550 200x100", window3_->bounds().ToString());
  }
}

// Assertions around dragging to the left/right edge of the screen.
TEST_F(WorkspaceWindowResizerTest, Edge) {
  window_->SetBounds(gfx::Rect(20, 30, 50, 60));
  {
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTCAPTION, 0, empty_windows()));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 10));
    resizer->CompleteDrag();
    EXPECT_EQ("0,0 400x600", window_->bounds().ToString());
    ASSERT_TRUE(GetRestoreBounds(window_.get()));
    EXPECT_EQ("20,30 50x60", GetRestoreBounds(window_.get())->ToString());
  }

  // Try the same with the right side.
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTCAPTION, 0, empty_windows()));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 800, 10));
  resizer->CompleteDrag();
  EXPECT_EQ("400,0 400x600", window_->bounds().ToString());
  ASSERT_TRUE(GetRestoreBounds(window_.get()));
  EXPECT_EQ("20,30 50x60", GetRestoreBounds(window_.get())->ToString());
}

}  // namespace
}  // namespace test
}  // namespace ash
