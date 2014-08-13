// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/partial_screenshot_view.h"

#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_overlay_delegate.h"
#include "ash/test/test_screenshot_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class PartialScreenshotViewTest : public test::AshTestBase,
                                  public views::WidgetObserver {
 public:
  PartialScreenshotViewTest() : view_(NULL) {}
  virtual ~PartialScreenshotViewTest() {
    if (view_)
      view_->GetWidget()->RemoveObserver(this);
  }

  void StartPartialScreenshot() {
    std::vector<PartialScreenshotView*> views =
        PartialScreenshotView::StartPartialScreenshot(GetScreenshotDelegate());
    if (!views.empty()) {
      view_ = views[0];
      view_->GetWidget()->AddObserver(this);
    }
  }

 protected:
  PartialScreenshotView* view_;

 private:
  // views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE {
    if (view_ && view_->GetWidget() == widget)
      view_ = NULL;
    widget->RemoveObserver(this);
  }

  DISALLOW_COPY_AND_ASSIGN(PartialScreenshotViewTest);
};

TEST_F(PartialScreenshotViewTest, BasicMouse) {
  StartPartialScreenshot();
  ASSERT_TRUE(view_);

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
  StartPartialScreenshot();
  ASSERT_TRUE(view_);

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

// Partial screenshot session should not start when there is already another
// overlay. See: http://crbug.com/341958
TEST_F(PartialScreenshotViewTest, DontStartOverOverlay) {
  OverlayEventFilter* overlay_filter = Shell::GetInstance()->overlay_filter();
  test::TestOverlayDelegate delegate;
  overlay_filter->Activate(&delegate);
  EXPECT_EQ(&delegate, overlay_filter->delegate_);

  StartPartialScreenshot();
  EXPECT_TRUE(view_ == NULL);
  EXPECT_EQ(&delegate, overlay_filter->delegate_);
  overlay_filter->Deactivate(&delegate);

  StartPartialScreenshot();
  EXPECT_TRUE(view_ != NULL);

  // Someone else attempts to activate the overlay session, which should cancel
  // the current partial screenshot session.
  overlay_filter->Activate(&delegate);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(&delegate, overlay_filter->delegate_);
  EXPECT_TRUE(view_ == NULL);
}

}  // namespace ash
