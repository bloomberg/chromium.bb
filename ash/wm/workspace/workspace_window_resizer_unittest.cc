// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer.h"

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/property_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/string_number_conversions.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/insets.h"

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
    aura::RootWindow* root = Shell::GetPrimaryRootWindow();
    root->SetHostSize(gfx::Size(800, kRootHeight));

    gfx::Rect root_bounds(root->bounds());
    EXPECT_EQ(kRootHeight, root_bounds.height());
    Shell::GetInstance()->SetDisplayWorkAreaInsets(root, gfx::Insets());
    window_.reset(new aura::Window(&delegate_));
    window_->Init(ui::LAYER_NOT_DRAWN);
    window_->SetParent(root);
    window_->set_id(1);

    window2_.reset(new aura::Window(&delegate2_));
    window2_->Init(ui::LAYER_NOT_DRAWN);
    window2_->SetParent(root);
    window2_->set_id(2);

    window3_.reset(new aura::Window(&delegate3_));
    window3_->Init(ui::LAYER_NOT_DRAWN);
    window3_->SetParent(root);
    window3_->set_id(3);
  }

  virtual void TearDown() OVERRIDE {
    window_.reset();
    window2_.reset();
    window3_.reset();
    AshTestBase::TearDown();
  }

  // Returns a string identifying the z-order of each of the known windows.
  // The returned string constains the id of the known windows and is ordered
  // from topmost to bottomost windows.
  std::string WindowOrderAsString() const {
    std::string result;
    const aura::Window::Windows& windows =
        Shell::GetPrimaryRootWindow()->children();
    for (aura::Window::Windows::const_reverse_iterator i = windows.rbegin();
         i != windows.rend(); ++i) {
      if (*i == window_.get() || *i == window2_.get() || *i == window3_.get()) {
        if (!result.empty())
          result += " ";
        result += base::IntToString((*i)->id());
      }
    }
    return result;
  }

 protected:
  void SetGridSize(int grid_size) {
    Shell::TestApi shell_test(Shell::GetInstance());
    shell_test.workspace_controller()->SetGridSize(grid_size);
  }
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

// TODO(oshima): Disabling these tests because these tests
// depends on the window to be specific size, but bots doesn't
// have enough space and the actual window gets smaller, which
// causing mismatch.
#if !defined(OS_WIN)

// Assertions around attached window resize dragging from the right with 2
// windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_2) {
  window_->SetBounds(gfx::Rect(0, 300, 400, 300));
  window2_->SetBounds(gfx::Rect(400, 200, 100, 200));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  SetGridSize(0);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the right, which should expand w1 and push w2.
  resizer->Drag(CalculateDragPoint(*resizer, 100, 10), 0);
  EXPECT_EQ("0,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("500,200 100x200", window2_->bounds().ToString());

  // Push off the screen, w2 should be resized to its min.
  delegate2_.set_min_size(gfx::Size(20, 20));
  resizer->Drag(CalculateDragPoint(*resizer, 800, 20), 0);
  EXPECT_EQ("0,300 780x300", window_->bounds().ToString());
  EXPECT_EQ("780,200 20x200", window2_->bounds().ToString());

  // Move back to 100 and verify w2 gets its original size.
  resizer->Drag(CalculateDragPoint(*resizer, 100, 10), 0);
  EXPECT_EQ("0,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("500,200 100x200", window2_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->Drag(CalculateDragPoint(*resizer, 800, 20), 0);
  resizer->RevertDrag();
  EXPECT_EQ("0,300 400x300", window_->bounds().ToString());
  EXPECT_EQ("400,200 100x200", window2_->bounds().ToString());
}

// Assertions around collapsing and expanding.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_Compress) {
  window_->SetBounds(gfx::Rect(   0, 300, 400, 300));
  window2_->SetBounds(gfx::Rect(400, 200, 100, 200));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  SetGridSize(0);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the left, which should expand w2 and collapse w1.
  resizer->Drag(CalculateDragPoint(*resizer, -100, 10), 0);
  EXPECT_EQ("0,300 300x300", window_->bounds().ToString());
  EXPECT_EQ("300,200 200x200", window2_->bounds().ToString());

  // Collapse all the way to w1's min.
  delegate_.set_min_size(gfx::Size(20, 20));
  resizer->Drag(CalculateDragPoint(*resizer, -800, 20), 0);
  EXPECT_EQ("0,300 20x300", window_->bounds().ToString());
  EXPECT_EQ("20,200 480x200", window2_->bounds().ToString());

  // Move 100 to the left.
  resizer->Drag(CalculateDragPoint(*resizer, 100, 10), 0);
  EXPECT_EQ("0,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("500,200 100x200", window2_->bounds().ToString());

  // Back to -100.
  resizer->Drag(CalculateDragPoint(*resizer, -100, 20), 0);
  EXPECT_EQ("0,300 300x300", window_->bounds().ToString());
  EXPECT_EQ("300,200 200x200", window2_->bounds().ToString());
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
  SetGridSize(10);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the right, which should expand w1 and push w2 and w3.
  resizer->Drag(CalculateDragPoint(*resizer, 100, -10), 0);
  EXPECT_EQ("100,300 300x300", window_->bounds().ToString());
  EXPECT_EQ("400,300 150x200", window2_->bounds().ToString());
  EXPECT_EQ("550,300 100x200", window3_->bounds().ToString());

  // Move it 296, which should now snap to grid and things should compress.
  resizer->Drag(CalculateDragPoint(*resizer, 296, -10), 0);
  EXPECT_EQ("100,300 500x300", window_->bounds().ToString());
  EXPECT_EQ("600,300 120x200", window2_->bounds().ToString());
  EXPECT_EQ("720,300 80x200", window3_->bounds().ToString());

  // Move it so much everything ends up at its min.
  resizer->Drag(CalculateDragPoint(*resizer, 798, 50), 0);
  EXPECT_EQ("100,300 600x300", window_->bounds().ToString());
  EXPECT_EQ("700,300 60x200", window2_->bounds().ToString());
  EXPECT_EQ("760,300 40x200", window3_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->RevertDrag();
  EXPECT_EQ("100,300 200x300", window_->bounds().ToString());
  EXPECT_EQ("300,300 150x200", window2_->bounds().ToString());
  EXPECT_EQ("450,300 100x200", window3_->bounds().ToString());
}

// Assertions around attached window resizing (collapsing and expanding) with
// 3 windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_RIGHT_3_Compress) {
  window_->SetBounds(gfx::Rect( 100, 300, 200, 300));
  window2_->SetBounds(gfx::Rect(300, 300, 200, 200));
  window3_->SetBounds(gfx::Rect(450, 300, 100, 200));
  delegate2_.set_min_size(gfx::Size(52, 50));
  delegate3_.set_min_size(gfx::Size(38, 50));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  SetGridSize(10);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTRIGHT, windows));
  ASSERT_TRUE(resizer.get());
  // Move it -100 to the right, which should collapse w1 and expand w2 and w3.
  resizer->Drag(CalculateDragPoint(*resizer, -100, -10), 0);
  EXPECT_EQ("100,300 100x300", window_->bounds().ToString());
  EXPECT_EQ("200,300 270x200", window2_->bounds().ToString());
  EXPECT_EQ("470,300 130x200", window3_->bounds().ToString());

  // Move it 100 to the right.
  resizer->Drag(CalculateDragPoint(*resizer, 100, -10), 0);
  EXPECT_EQ("100,300 300x300", window_->bounds().ToString());
  EXPECT_EQ("400,300 200x200", window2_->bounds().ToString());
  EXPECT_EQ("600,300 100x200", window3_->bounds().ToString());

  // 100 to the left again.
  resizer->Drag(CalculateDragPoint(*resizer, -100, -10), 0);
  EXPECT_EQ("100,300 100x300", window_->bounds().ToString());
  EXPECT_EQ("200,300 270x200", window2_->bounds().ToString());
  EXPECT_EQ("470,300 130x200", window3_->bounds().ToString());
}

// Assertions around collapsing and expanding from the bottom.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_Compress) {
  window_->SetBounds(gfx::Rect(   0, 100, 400, 300));
  window2_->SetBounds(gfx::Rect(400, 400, 100, 200));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  SetGridSize(0);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTBOTTOM, windows));
  ASSERT_TRUE(resizer.get());
  // Move it up 100, which should expand w2 and collapse w1.
  resizer->Drag(CalculateDragPoint(*resizer, 10, -100), 0);
  EXPECT_EQ("0,100 400x200", window_->bounds().ToString());
  EXPECT_EQ("400,300 100x300", window2_->bounds().ToString());

  // Collapse all the way to w1's min.
  delegate_.set_min_size(gfx::Size(20, 20));
  resizer->Drag(CalculateDragPoint(*resizer, 20, -800), 0);
  EXPECT_EQ("0,100 400x20", window_->bounds().ToString());
  EXPECT_EQ("400,120 100x480", window2_->bounds().ToString());

  // Move 100 down.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 100), 0);
  EXPECT_EQ("0,100 400x400", window_->bounds().ToString());
  EXPECT_EQ("400,500 100x100", window2_->bounds().ToString());

  // Back to -100.
  resizer->Drag(CalculateDragPoint(*resizer, 20, -100), 0);
  EXPECT_EQ("0,100 400x200", window_->bounds().ToString());
  EXPECT_EQ("400,300 100x300", window2_->bounds().ToString());
}

// Assertions around attached window resize dragging from the bottom with 2
// windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_2) {
  window_->SetBounds(gfx::Rect( 0,  50, 400, 200));
  window2_->SetBounds(gfx::Rect(0, 250, 200, 100));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  SetGridSize(0);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTBOTTOM, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the bottom, which should expand w1 and push w2.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 100), 0);
  EXPECT_EQ("0,50 400x300", window_->bounds().ToString());
  EXPECT_EQ("0,350 200x100", window2_->bounds().ToString());

  // Push off the screen, w2 should be resized to its min.
  delegate2_.set_min_size(gfx::Size(20, 20));
  resizer->Drag(CalculateDragPoint(*resizer, 50, 820), 0);
  EXPECT_EQ("0,50 400x530", window_->bounds().ToString());
  EXPECT_EQ("0,580 200x20", window2_->bounds().ToString());

  // Move back to 100 and verify w2 gets its original size.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 100), 0);
  EXPECT_EQ("0,50 400x300", window_->bounds().ToString());
  EXPECT_EQ("0,350 200x100", window2_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->Drag(CalculateDragPoint(*resizer, 800, 20), 0);
  resizer->RevertDrag();
  EXPECT_EQ("0,50 400x200", window_->bounds().ToString());
  EXPECT_EQ("0,250 200x100", window2_->bounds().ToString());
}

// Assertions around attached window resize dragging from the bottom with 3
// windows.
// TODO(oshima): Host window doesn't get a resize event after
// SetHostSize on Windows trybot, which gives wrong work/display area.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_3) {
  aura::RootWindow* root = Shell::GetPrimaryRootWindow();
  root->SetHostSize(gfx::Size(600, 800));
  LOG(ERROR) << "=== Calling OnHostResized, 600x800";

  Shell::GetInstance()->SetDisplayWorkAreaInsets(root, gfx::Insets());

  window_->SetBounds(gfx::Rect( 300, 100, 300, 200));
  window2_->SetBounds(gfx::Rect(300, 300, 200, 150));
  window3_->SetBounds(gfx::Rect(300, 450, 200, 100));
  delegate2_.set_min_size(gfx::Size(50, 52));
  delegate3_.set_min_size(gfx::Size(50, 38));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  SetGridSize(10);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTBOTTOM, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 to the right, which should expand w1 and push w2 and w3.
  resizer->Drag(CalculateDragPoint(*resizer, -10, 100), 0);
  EXPECT_EQ("300,100 300x300", window_->bounds().ToString());
  EXPECT_EQ("300,400 200x150", window2_->bounds().ToString());
  EXPECT_EQ("300,550 200x100", window3_->bounds().ToString());

  // Move it 296, which should now snap to grid and things should compress.
  resizer->Drag(CalculateDragPoint(*resizer, -10, 296), 0);
  EXPECT_EQ("300,100 300x500", window_->bounds().ToString());
  EXPECT_EQ("300,600 200x120", window2_->bounds().ToString());
  EXPECT_EQ("300,720 200x80", window3_->bounds().ToString());

  // Move it so much everything ends up at its min.
  resizer->Drag(CalculateDragPoint(*resizer, 50, 798), 0);
  EXPECT_EQ("300,100 300x600", window_->bounds().ToString());
  EXPECT_EQ("300,700 200x60", window2_->bounds().ToString());
  EXPECT_EQ("300,760 200x40", window3_->bounds().ToString());

  // Revert and make sure everything moves back.
  resizer->RevertDrag();
  EXPECT_EQ("300,100 300x200", window_->bounds().ToString());
  EXPECT_EQ("300,300 200x150", window2_->bounds().ToString());
  EXPECT_EQ("300,450 200x100", window3_->bounds().ToString());
}

// Assertions around attached window resizing (collapsing and expanding) with
// 3 windows.
TEST_F(WorkspaceWindowResizerTest, AttachedResize_BOTTOM_3_Compress) {
  window_->SetBounds(gfx::Rect(  0,   0, 200, 200));
  window2_->SetBounds(gfx::Rect(10, 200, 200, 200));
  window3_->SetBounds(gfx::Rect(20, 400, 100, 100));
  delegate2_.set_min_size(gfx::Size(52, 50));
  delegate3_.set_min_size(gfx::Size(38, 50));

  std::vector<aura::Window*> windows;
  windows.push_back(window2_.get());
  windows.push_back(window3_.get());
  SetGridSize(10);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTBOTTOM, windows));
  ASSERT_TRUE(resizer.get());
  // Move it 100 up, which should collapse w1 and expand w2 and w3.
  resizer->Drag(CalculateDragPoint(*resizer, -10, -100), 0);
  EXPECT_EQ("0,0 200x100", window_->bounds().ToString());
  EXPECT_EQ("10,100 200x270", window2_->bounds().ToString());
  EXPECT_EQ("20,370 100x130", window3_->bounds().ToString());

  // Move it 100 down.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 100), 0);
  EXPECT_EQ("0,0 200x300", window_->bounds().ToString());
  EXPECT_EQ("10,300 200x200", window2_->bounds().ToString());
  EXPECT_EQ("20,500 100x100", window3_->bounds().ToString());

  // 100 up again.
  resizer->Drag(CalculateDragPoint(*resizer, -10, -100), 0);
  EXPECT_EQ("0,0 200x100", window_->bounds().ToString());
  EXPECT_EQ("10,100 200x270", window2_->bounds().ToString());
  EXPECT_EQ("20,370 100x130", window3_->bounds().ToString());
}

// Assertions around dragging to the left/right edge of the screen.
TEST_F(WorkspaceWindowResizerTest, Edge) {
  int bottom =
      ScreenAsh::GetUnmaximizedWorkAreaBounds(window_.get()).bottom();
  window_->SetBounds(gfx::Rect(20, 30, 50, 60));
  {
    SetGridSize(0);
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTCAPTION, empty_windows()));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
    resizer->CompleteDrag(0);
    EXPECT_EQ("0,0 400x" + base::IntToString(bottom),
              window_->bounds().ToString());
    ASSERT_TRUE(GetRestoreBounds(window_.get()));
    EXPECT_EQ("20,30 50x60", GetRestoreBounds(window_.get())->ToString());
  }

  // Try the same with the right side.
  SetGridSize(0);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTCAPTION, empty_windows()));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 800, 10), 0);
  resizer->CompleteDrag(0);
  EXPECT_EQ("400,0 400x" + base::IntToString(bottom),
            window_->bounds().ToString());
  ASSERT_TRUE(GetRestoreBounds(window_.get()));
  EXPECT_EQ("20,30 50x60", GetRestoreBounds(window_.get())->ToString());
}

// Verifies windows are correctly restacked when reordering multiple windows.
TEST_F(WorkspaceWindowResizerTest, RestackAttached) {
  window_->SetBounds(gfx::Rect(   0, 0, 200, 300));
  window2_->SetBounds(gfx::Rect(200, 0, 100, 200));
  window3_->SetBounds(gfx::Rect(300, 0, 100, 100));

  {
    std::vector<aura::Window*> windows;
    windows.push_back(window2_.get());
    SetGridSize(10);
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window_.get(), gfx::Point(), HTRIGHT, windows));
    ASSERT_TRUE(resizer.get());
    // Move it 100 to the right, which should expand w1 and push w2 and w3.
    resizer->Drag(CalculateDragPoint(*resizer, 100, -10), 0);

    // 2 should be topmost since it's initially the highest in the stack.
    EXPECT_EQ("2 1 3", WindowOrderAsString());
  }

  {
    std::vector<aura::Window*> windows;
    windows.push_back(window3_.get());
    SetGridSize(10);
    scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
        window2_.get(), gfx::Point(), HTRIGHT, windows));
    ASSERT_TRUE(resizer.get());
    // Move it 100 to the right, which should expand w1 and push w2 and w3.
    resizer->Drag(CalculateDragPoint(*resizer, 100, -10), 0);

    // 2 should be topmost since it's initially the highest in the stack.
    EXPECT_EQ("2 3 1", WindowOrderAsString());
  }
}

// Makes sure we don't allow dragging below the work area.
TEST_F(WorkspaceWindowResizerTest, DontDragOffBottom) {
  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      Shell::GetPrimaryRootWindow(), gfx::Insets(0, 0, 10, 0));

  window_->SetBounds(gfx::Rect(100, 200, 300, 400));
  SetGridSize(0);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTCAPTION, empty_windows()));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 600), 0);
  int expected_y =
      kRootHeight - WorkspaceWindowResizer::kMinOnscreenHeight - 10;
  EXPECT_EQ("100," + base::IntToString(expected_y) + " 300x400",
            window_->bounds().ToString());
}

// Makes sure we don't allow dragging off the top of the work area.
TEST_F(WorkspaceWindowResizerTest, DontDragOffTop) {
  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      Shell::GetPrimaryRootWindow(), gfx::Insets(10, 0, 0, 0));

  window_->SetBounds(gfx::Rect(100, 200, 300, 400));
  SetGridSize(0);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTCAPTION, empty_windows()));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 0, -600), 0);
  EXPECT_EQ("100,10 300x400", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, ResizeBottomOutsideWorkArea) {
  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      Shell::GetPrimaryRootWindow(), gfx::Insets(0, 0, 50, 0));

  window_->SetBounds(gfx::Rect(100, 200, 300, 380));
  SetGridSize(10);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTTOP, empty_windows()));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 8, 0), 0);
  EXPECT_EQ("100,200 300x380", window_->bounds().ToString());
}

// Verifies snapping to edges works.
TEST_F(WorkspaceWindowResizerTest, SnapToEdge) {
  Shell::GetInstance()->SetShelfAutoHideBehavior(
      SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  window_->SetBounds(gfx::Rect(96, 112, 320, 160));
  SetGridSize(16);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTCAPTION, empty_windows()));
  ASSERT_TRUE(resizer.get());
  // Move to an x-coordinate of 15, which should not snap.
  resizer->Drag(CalculateDragPoint(*resizer, -81, 0), 0);
  // An x-coordinate of 7 should snap.
  resizer->Drag(CalculateDragPoint(*resizer, -89, 0), 0);
  EXPECT_EQ("0,112 320x160", window_->bounds().ToString());
  // Move to -20, should still snap to 0.
  resizer->Drag(CalculateDragPoint(*resizer, -116, 0), 0);
  EXPECT_EQ("0,112 320x160", window_->bounds().ToString());
  // At -32 should move past snap points.
  resizer->Drag(CalculateDragPoint(*resizer, -128, 0), 0);
  EXPECT_EQ("-32,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, -129, 0), 0);
  EXPECT_EQ("-33,112 320x160", window_->bounds().ToString());

  // Right side should similarly snap.
  resizer->Drag(CalculateDragPoint(*resizer, 800 - 320 - 96 - 15, 0), 0);
  EXPECT_EQ("465,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 800 - 320 - 96 - 7, 0), 0);
  EXPECT_EQ("480,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 800 - 320 - 96 + 20, 0), 0);
  EXPECT_EQ("480,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 800 - 320 - 96 + 32, 0), 0);
  EXPECT_EQ("512,112 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 800 - 320 - 96 + 33, 0), 0);
  EXPECT_EQ("513,112 320x160", window_->bounds().ToString());

  // And the bottom should snap too.
  resizer->Drag(CalculateDragPoint(*resizer, 0, 600 - 160 - 112 - 15), 0);
  EXPECT_EQ("96,432 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 600 - 160 - 112 + 20), 0);
  EXPECT_EQ("96,432 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 600 - 160 - 112 + 32), 0);
  EXPECT_EQ("96,472 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 0, 600 - 160 - 112 + 33), 0);
  EXPECT_EQ("96,473 320x160", window_->bounds().ToString());

  // And the top should snap too.
  resizer->Drag(CalculateDragPoint(*resizer, 0, -112 + 20), 0);
  EXPECT_EQ("96,20 320x160", window_->bounds().ToString());
  resizer->Drag(CalculateDragPoint(*resizer, 0, -112 + 7), 0);
  EXPECT_EQ("96,0 320x160", window_->bounds().ToString());
  // No need to test dragging < 0 as we force that to 0.
}

// Verifies a window taller than work area height doesn't snap above the top of
// the work area.
TEST_F(WorkspaceWindowResizerTest, TallWindow) {
  aura::RootWindow* root = Shell::GetPrimaryRootWindow();
  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      root, gfx::Insets(0, 0, 50, 0));
  window_->SetBounds(gfx::Rect(0, 0, 320, 560));
  SetGridSize(16);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTCAPTION, empty_windows()));
  resizer->Drag(CalculateDragPoint(*resizer, 0, 9), 0);
  EXPECT_EQ("0,9 320x560", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, DragResizeSnapToGrid) {
  window_->SetBounds(gfx::Rect(96, 112, 320, 160));
  SetGridSize(16);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTBOTTOMRIGHT, empty_windows()));
  ASSERT_TRUE(resizer.get());
  // Resize the right bottom to add 10 in width, 12 in height.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 12), 0);
  // Both bottom and right sides should snap to grids.
  EXPECT_EQ("96,112 336x176", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, CtrlDragResizeToExactPosition) {
  window_->SetBounds(gfx::Rect(96, 112, 320, 160));
  SetGridSize(16);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTBOTTOMRIGHT, empty_windows()));
  ASSERT_TRUE(resizer.get());
  // Resize the right bottom to add 10 in width, 12 in height.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 12), ui::EF_CONTROL_DOWN);
  // Both bottom and right sides to resize to exact size requested.
  EXPECT_EQ("96,112 330x172", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, CompleteDragMoveSnapToGrid) {
  window_->SetBounds(gfx::Rect(96, 112, 320, 160));
  SetGridSize(16);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTCAPTION, empty_windows()));
  ASSERT_TRUE(resizer.get());
  // Drag the window to new poistion by adding (10, 12) to original point,
  // the window should snap to the closed grid.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 12), 0);
  resizer->CompleteDrag(0);
  EXPECT_EQ("112,128 320x160", window_->bounds().ToString());
}

TEST_F(WorkspaceWindowResizerTest, CtrlCompleteDragMoveToExactPosition) {
  window_->SetBounds(gfx::Rect(96, 112, 320, 160));
  SetGridSize(16);
  scoped_ptr<WorkspaceWindowResizer> resizer(WorkspaceWindowResizer::Create(
      window_.get(), gfx::Point(), HTCAPTION, empty_windows()));
  ASSERT_TRUE(resizer.get());
  // Ctrl + drag the window to new poistion by adding (10, 12) to its origin,
  // the window should move to the exact position.
  resizer->Drag(CalculateDragPoint(*resizer, 10, 12), 0);
  resizer->CompleteDrag(ui::EF_CONTROL_DOWN);
  EXPECT_EQ("106,124 320x160", window_->bounds().ToString());
}

#endif

}  // namespace
}  // namespace test
}  // namespace ash
