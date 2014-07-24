// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/partial_screenshot_view.h"

#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_screenshot_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class PartialScreenshotViewTest : public test::AshTestBase {
 public:
  PartialScreenshotViewTest() : view_(NULL) {}
  virtual ~PartialScreenshotViewTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    std::vector<PartialScreenshotView*> views =
        PartialScreenshotView::StartPartialScreenshot(GetScreenshotDelegate());
    ASSERT_EQ(1u, views.size());
    view_ = views[0];
  }

 protected:
  PartialScreenshotView* view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PartialScreenshotViewTest);
};

TEST_F(PartialScreenshotViewTest, BasicMouse) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.MoveMouseTo(100, 100);
  generator.PressLeftButton();
  EXPECT_FALSE(view_->is_dragging_);
  EXPECT_EQ("100,100", view_->start_position_.ToString());

  generator.MoveMouseTo(200, 200);
  EXPECT_TRUE(view_->is_dragging_);
  EXPECT_EQ("200,200", view_->current_position_.ToString());

  generator.ReleaseLeftButton();
  EXPECT_FALSE(view_->is_dragging_);
  EXPECT_EQ("100,100 100x100", GetScreenshotDelegate()->last_rect().ToString());
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());
}

TEST_F(PartialScreenshotViewTest, BasicTouch) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  generator.set_current_location(gfx::Point(100,100));
  generator.GestureTapDownAndUp(gfx::Point(100,100));
  EXPECT_FALSE(view_->is_dragging_);
  EXPECT_EQ(0, GetScreenshotDelegate()->handle_take_partial_screenshot_count());

  generator.PressTouch();
  EXPECT_FALSE(view_->is_dragging_);
  EXPECT_EQ("100,100", view_->start_position_.ToString());

  generator.MoveTouch(gfx::Point(200, 200));
  EXPECT_TRUE(view_->is_dragging_);
  EXPECT_EQ("200,200", view_->current_position_.ToString());

  generator.ReleaseTouch();
  EXPECT_FALSE(view_->is_dragging_);
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());
  EXPECT_EQ("100,100 100x100", GetScreenshotDelegate()->last_rect().ToString());
}

}  // namespace ash
