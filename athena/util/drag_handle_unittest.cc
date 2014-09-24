// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/util/drag_handle.h"

#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace athena {

class DragHandleDelegateTest : public DragHandleScrollDelegate {
 public:
  explicit DragHandleDelegateTest()
      : begin_delta_(0),
        got_scroll_end_(false),
        update_delta_(0) {}

  virtual ~DragHandleDelegateTest() {}

  void Reset() {
    begin_delta_ = 0;
    got_scroll_end_ = false;
    update_delta_ = 0;
  }

  float begin_delta() { return begin_delta_; }
  bool got_scroll_end() { return got_scroll_end_; }
  float update_delta() { return update_delta_; }

 private:
  // DragHandleScrollDelegate:
  virtual void HandleScrollBegin(float delta) OVERRIDE {
    begin_delta_ = delta;
  }

  virtual void HandleScrollEnd() OVERRIDE {
    got_scroll_end_ = true;
  }

  virtual void HandleScrollUpdate(float delta) OVERRIDE {
    update_delta_ = delta;
  }

  float begin_delta_;
  bool got_scroll_end_;
  float update_delta_;

  DISALLOW_COPY_AND_ASSIGN(DragHandleDelegateTest);
};

typedef aura::test::AuraTestBase DragHandleTest;

const int kDragHandleWidth = 10;
const int kDragHandleHeight = 100;

ui::GestureEvent CreateGestureEvent(ui::EventType type,
                                    float x,
                                    float delta_x) {
  ui::GestureEvent event(
      x,
      1,
      0,
      base::TimeDelta::FromInternalValue(base::Time::Now().ToInternalValue()),
      type == ui::ET_GESTURE_SCROLL_END
          ? ui::GestureEventDetails(type)
          : ui::GestureEventDetails(type, delta_x, 0));
  event.set_root_location(gfx::PointF(x, 1));
  return event;
}

TEST_F(DragHandleTest, ScrollTest) {
  DragHandleDelegateTest delegate;
  scoped_ptr<views::View> drag_handle(
      CreateDragHandleView(DragHandleScrollDirection::DRAG_HANDLE_HORIZONTAL,
                           &delegate,
                           kDragHandleWidth,
                           kDragHandleHeight));

  views::Widget widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = root_window();
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(params);
  widget.SetContentsView(drag_handle.get());
  const gfx::Size& size = gfx::Size(kDragHandleWidth, kDragHandleHeight);
  widget.SetSize(size);
  widget.SetBounds(gfx::Rect(0, 0, size.width(), size.height()));

  const float begin_x = 4.0;
  const float begin_delta = 10.0;
  const float update_delta = 15.0;

  ui::GestureEvent scroll_begin_model =
      CreateGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN, begin_x, begin_delta);
  ui::GestureEvent scroll_update_model =
      CreateGestureEvent(ui::ET_GESTURE_SCROLL_UPDATE,
                         begin_x + update_delta,
                         update_delta - begin_delta);
  ui::GestureEvent scroll_end_model =
      CreateGestureEvent(ui::ET_GESTURE_SCROLL_END, begin_x + update_delta, 0);
  ui::GestureEvent fling_start_model =
      CreateGestureEvent(ui::ET_SCROLL_FLING_START, begin_x + update_delta, 0);

  // Normal scroll
  ui::GestureEvent e(scroll_begin_model);
  widget.OnGestureEvent(&e);
  EXPECT_EQ(begin_delta, delegate.begin_delta());
  EXPECT_EQ(0, delegate.update_delta());
  EXPECT_FALSE(delegate.got_scroll_end());
  e = ui::GestureEvent(scroll_update_model);
  widget.OnGestureEvent(&e);
  EXPECT_EQ(update_delta, delegate.update_delta());
  EXPECT_FALSE(delegate.got_scroll_end());
  e = ui::GestureEvent(scroll_end_model);
  widget.OnGestureEvent(&e);
  EXPECT_EQ(update_delta, delegate.update_delta());
  EXPECT_TRUE(delegate.got_scroll_end());

  delegate.Reset();

  // Scroll ending with a fling
  e = ui::GestureEvent(scroll_begin_model);
  widget.OnGestureEvent(&e);
  e = ui::GestureEvent(scroll_update_model);
  widget.OnGestureEvent(&e);
  e = ui::GestureEvent(fling_start_model);
  widget.OnGestureEvent(&e);
  EXPECT_TRUE(delegate.got_scroll_end());

  delegate.Reset();

  drag_handle.reset();
}

}  // namespace athena
