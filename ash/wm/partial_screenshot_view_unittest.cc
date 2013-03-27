// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/partial_screenshot_view.h"

#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"

namespace ash {

class FakeScreenshotDelegate : public ScreenshotDelegate {
 public:
  FakeScreenshotDelegate() : screenshot_count_(0) {}

  void HandleTakeScreenshotForAllRootWindows() OVERRIDE {}
  void HandleTakePartialScreenshot(aura::Window* window,
                                   const gfx::Rect& rect) OVERRIDE {
    rect_ = rect;
    screenshot_count_++;
  }

  bool CanTakeScreenshot() OVERRIDE {
    return true;
  }

  const gfx::Rect& rect() const { return rect_; };
  int screenshot_count() const { return screenshot_count_; };

 private:
  gfx::Rect rect_;
  int screenshot_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeScreenshotDelegate);
};

class PartialScreenshotViewTest : public test::AshTestBase {
 public:
  PartialScreenshotViewTest() : view_(NULL) {}
  virtual ~PartialScreenshotViewTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    delegate_.reset(new FakeScreenshotDelegate());
    std::vector<PartialScreenshotView*> views =
        PartialScreenshotView::StartPartialScreenshot(delegate_.get());
    ASSERT_EQ(1u, views.size());
    view_ = views[0];
  }

 protected:
  PartialScreenshotView* view_;
  scoped_ptr<FakeScreenshotDelegate> delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PartialScreenshotViewTest);
};

TEST_F(PartialScreenshotViewTest, BasicMouse) {
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.MoveMouseTo(100, 100);
  generator.PressLeftButton();
  EXPECT_FALSE(view_->is_dragging_);
  EXPECT_EQ("100,100", view_->start_position_.ToString());

  generator.MoveMouseTo(200, 200);
  EXPECT_TRUE(view_->is_dragging_);
  EXPECT_EQ("200,200", view_->current_position_.ToString());

  generator.ReleaseLeftButton();
  EXPECT_FALSE(view_->is_dragging_);
  EXPECT_EQ("100,100 100x100", delegate_->rect().ToString());
  EXPECT_EQ(1, delegate_->screenshot_count());
}

TEST_F(PartialScreenshotViewTest, BasicTouch) {
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.set_current_location(gfx::Point(100,100));
  generator.GestureTapDownAndUp(gfx::Point(100,100));
  EXPECT_FALSE(view_->is_dragging_);
  EXPECT_EQ(0, delegate_->screenshot_count());

  generator.PressTouch();
  EXPECT_FALSE(view_->is_dragging_);
  EXPECT_EQ("100,100", view_->start_position_.ToString());

  generator.MoveTouch(gfx::Point(200, 200));
  EXPECT_TRUE(view_->is_dragging_);
  EXPECT_EQ("200,200", view_->current_position_.ToString());

  generator.ReleaseTouch();
  EXPECT_FALSE(view_->is_dragging_);
  EXPECT_EQ(1, delegate_->screenshot_count());
  EXPECT_EQ("100,100 100x100", delegate_->rect().ToString());
}

}  // namespace ash
