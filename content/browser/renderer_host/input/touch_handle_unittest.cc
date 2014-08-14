// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_handle.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/mock_motion_event.h"
#include "ui/gfx/geometry/rect_f.h"

using ui::test::MockMotionEvent;

namespace content {
namespace {

const float kDefaultDrawableSize = 10.f;

struct MockDrawableData {
  MockDrawableData()
      : orientation(TOUCH_HANDLE_ORIENTATION_UNDEFINED),
        alpha(0.f),
        enabled(false),
        visible(false),
        rect(0, 0, kDefaultDrawableSize, kDefaultDrawableSize) {}
  TouchHandleOrientation orientation;
  float alpha;
  bool enabled;
  bool visible;
  gfx::RectF rect;
};

class MockTouchHandleDrawable : public TouchHandleDrawable {
 public:
  explicit MockTouchHandleDrawable(MockDrawableData* data) : data_(data) {}
  virtual ~MockTouchHandleDrawable() {}

  virtual void SetEnabled(bool enabled) OVERRIDE { data_->enabled = enabled; }

  virtual void SetOrientation(TouchHandleOrientation orientation) OVERRIDE {
    data_->orientation = orientation;
  }

  virtual void SetAlpha(float alpha) OVERRIDE { data_->alpha = alpha; }

  virtual void SetFocus(const gfx::PointF& position) OVERRIDE {
    // Anchor focus to the top left of the rect (regardless of orientation).
    data_->rect.set_origin(position);
  }

  virtual void SetVisible(bool visible) OVERRIDE { data_->visible = visible; }

  virtual bool IntersectsWith(const gfx::RectF& rect) const OVERRIDE {
    return data_->rect.Intersects(rect);
  }

 private:
  MockDrawableData* data_;
};

}  // namespace

class TouchHandleTest : public testing::Test, public TouchHandleClient {
 public:
  TouchHandleTest()
      : dragging_(false),
        dragged_(false),
        tapped_(false),
        needs_animate_(false) {}

  virtual ~TouchHandleTest() {}

  // TouchHandleClient implementation.
  virtual void OnHandleDragBegin(const TouchHandle& handle) OVERRIDE {
    dragging_ = true;
  }

  virtual void OnHandleDragUpdate(const TouchHandle& handle,
                                  const gfx::PointF& new_position) OVERRIDE {
    dragged_ = true;
    drag_position_ = new_position;
  }

  virtual void OnHandleDragEnd(const TouchHandle& handle) OVERRIDE {
    dragging_ = false;
  }

  virtual void OnHandleTapped(const TouchHandle& handle) OVERRIDE {
    tapped_ = true;
  }

  virtual void SetNeedsAnimate() OVERRIDE { needs_animate_ = true; }

  virtual scoped_ptr<TouchHandleDrawable> CreateDrawable() OVERRIDE {
    return scoped_ptr<TouchHandleDrawable>(
        new MockTouchHandleDrawable(&drawable_data_));
  }

  void Animate(TouchHandle& handle) {
    needs_animate_ = false;
    base::TimeTicks now = base::TimeTicks::Now();
    while (handle.Animate(now))
      now += base::TimeDelta::FromMilliseconds(16);
  }

  bool GetAndResetHandleDragged() {
    bool dragged = dragged_;
    dragged_ = false;
    return dragged;
  }

  bool GetAndResetHandleTapped() {
    bool tapped = tapped_;
    tapped_ = false;
    return tapped;
  }

  bool GetAndResetNeedsAnimate() {
    bool needs_animate = needs_animate_;
    needs_animate_ = false;
    return needs_animate;
  }

  bool IsDragging() const { return dragging_; }
  const gfx::PointF& DragPosition() const { return drag_position_; }
  bool NeedsAnimate() const { return needs_animate_; }

  const MockDrawableData& drawable() { return drawable_data_; }

 private:
  gfx::PointF drag_position_;
  bool dragging_;
  bool dragged_;
  bool tapped_;
  bool needs_animate_;

  MockDrawableData drawable_data_;
};

TEST_F(TouchHandleTest, Visibility) {
  TouchHandle handle(this, TOUCH_HANDLE_CENTER);
  EXPECT_FALSE(drawable().visible);

  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);

  handle.SetVisible(false, TouchHandle::ANIMATION_NONE);
  EXPECT_FALSE(drawable().visible);

  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);
}

TEST_F(TouchHandleTest, VisibilityAnimation) {
  TouchHandle handle(this, TOUCH_HANDLE_CENTER);
  ASSERT_FALSE(NeedsAnimate());
  ASSERT_FALSE(drawable().visible);
  ASSERT_EQ(0.f, drawable().alpha);

  handle.SetVisible(true, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_TRUE(NeedsAnimate());
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(0.f, drawable().alpha);

  Animate(handle);
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);

  ASSERT_FALSE(NeedsAnimate());
  handle.SetVisible(false, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_TRUE(NeedsAnimate());
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);

  Animate(handle);
  EXPECT_FALSE(drawable().visible);
  EXPECT_EQ(0.f, drawable().alpha);

  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);
  EXPECT_EQ(1.f, drawable().alpha);
  EXPECT_FALSE(GetAndResetNeedsAnimate());
  handle.SetVisible(false, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_EQ(1.f, drawable().alpha);
  EXPECT_TRUE(GetAndResetNeedsAnimate());
  handle.SetVisible(true, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_EQ(1.f, drawable().alpha);
  EXPECT_FALSE(GetAndResetNeedsAnimate());
}

TEST_F(TouchHandleTest, Orientation) {
  TouchHandle handle(this, TOUCH_HANDLE_CENTER);
  EXPECT_EQ(TOUCH_HANDLE_CENTER, drawable().orientation);

  handle.SetOrientation(TOUCH_HANDLE_LEFT);
  EXPECT_EQ(TOUCH_HANDLE_LEFT, drawable().orientation);

  handle.SetOrientation(TOUCH_HANDLE_RIGHT);
  EXPECT_EQ(TOUCH_HANDLE_RIGHT, drawable().orientation);

  handle.SetOrientation(TOUCH_HANDLE_CENTER);
  EXPECT_EQ(TOUCH_HANDLE_CENTER, drawable().orientation);
}

TEST_F(TouchHandleTest, Position) {
  TouchHandle handle(this, TOUCH_HANDLE_CENTER);
  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);

  gfx::PointF position;
  EXPECT_EQ(gfx::PointF(), drawable().rect.origin());

  position = gfx::PointF(7.3f, -3.7f);
  handle.SetPosition(position);
  EXPECT_EQ(position, drawable().rect.origin());

  position = gfx::PointF(-7.3f, 3.7f);
  handle.SetPosition(position);
  EXPECT_EQ(position, drawable().rect.origin());
}

TEST_F(TouchHandleTest, PositionNotUpdatedWhileFadingOrInvisible) {
  TouchHandle handle(this, TOUCH_HANDLE_CENTER);

  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);
  ASSERT_TRUE(drawable().visible);
  ASSERT_FALSE(NeedsAnimate());

  gfx::PointF old_position(7.3f, -3.7f);
  handle.SetPosition(old_position);
  ASSERT_EQ(old_position, drawable().rect.origin());

  handle.SetVisible(false, TouchHandle::ANIMATION_SMOOTH);
  ASSERT_TRUE(NeedsAnimate());

  gfx::PointF new_position(3.7f, -3.7f);
  handle.SetPosition(new_position);
  EXPECT_EQ(old_position, drawable().rect.origin());
  EXPECT_TRUE(NeedsAnimate());

  // While the handle is fading, the new position should not take affect.
  base::TimeTicks now = base::TimeTicks::Now();
  while (handle.Animate(now)) {
    EXPECT_EQ(old_position, drawable().rect.origin());
    now += base::TimeDelta::FromMilliseconds(16);
  }

  // Even after the animation terminates, the new position will not be pushed.
  EXPECT_EQ(old_position, drawable().rect.origin());

  // As soon as the handle becomes visible, the new position will be pushed.
  handle.SetVisible(true, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_EQ(new_position, drawable().rect.origin());
}

TEST_F(TouchHandleTest, Enabled) {
  // A newly created handle defaults to enabled.
  TouchHandle handle(this, TOUCH_HANDLE_CENTER);
  EXPECT_TRUE(drawable().enabled);

  handle.SetVisible(true, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_TRUE(GetAndResetNeedsAnimate());
  EXPECT_EQ(0.f, drawable().alpha);
  handle.SetEnabled(false);
  EXPECT_FALSE(drawable().enabled);

  // Dragging should not be allowed while the handle is disabled.
  base::TimeTicks event_time = base::TimeTicks::Now();
  const float kOffset = kDefaultDrawableSize / 2.f;
  MockMotionEvent event(
      MockMotionEvent::ACTION_DOWN, event_time, kOffset, kOffset);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));

  // Disabling mid-animation should cancel the animation.
  handle.SetEnabled(true);
  handle.SetVisible(false, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_TRUE(drawable().visible);
  EXPECT_TRUE(GetAndResetNeedsAnimate());
  handle.SetEnabled(false);
  EXPECT_FALSE(drawable().enabled);
  EXPECT_FALSE(drawable().visible);
  EXPECT_FALSE(handle.Animate(base::TimeTicks::Now()));

  // Disabling mid-drag should cancel the drag.
  handle.SetEnabled(true);
  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());
  handle.SetEnabled(false);
  EXPECT_FALSE(IsDragging());
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
}

TEST_F(TouchHandleTest, Drag) {
  TouchHandle handle(this, TOUCH_HANDLE_CENTER);

  base::TimeTicks event_time = base::TimeTicks::Now();
  const float kOffset = kDefaultDrawableSize / 2.f;

  // The handle must be visible to trigger drag.
  MockMotionEvent event(
      MockMotionEvent::ACTION_DOWN, event_time, kOffset, kOffset);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());
  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);

  // ACTION_DOWN must fall within the drawable region to trigger drag.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time, 50, 50);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  // Only ACTION_DOWN will trigger drag.
  event = MockMotionEvent(
      MockMotionEvent::ACTION_MOVE, event_time, kOffset, kOffset);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  // Start the drag.
  event = MockMotionEvent(
      MockMotionEvent::ACTION_DOWN, event_time, kOffset, kOffset);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  event = MockMotionEvent(
      MockMotionEvent::ACTION_MOVE, event_time, kOffset + 10, kOffset + 15);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetHandleDragged());
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(gfx::PointF(10, 15), DragPosition());

  event = MockMotionEvent(
      MockMotionEvent::ACTION_MOVE, event_time, kOffset - 10, kOffset - 15);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetHandleDragged());
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(gfx::PointF(-10, -15), DragPosition());

  event = MockMotionEvent(MockMotionEvent::ACTION_UP);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleDragged());
  EXPECT_FALSE(IsDragging());

  // Non-ACTION_DOWN events after the drag has terminated should not be handled.
  event = MockMotionEvent(MockMotionEvent::ACTION_CANCEL);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
}

TEST_F(TouchHandleTest, DragDefersOrientationChange) {
  TouchHandle handle(this, TOUCH_HANDLE_RIGHT);
  ASSERT_EQ(drawable().orientation, TOUCH_HANDLE_RIGHT);
  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);

  MockMotionEvent event(MockMotionEvent::ACTION_DOWN);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  // Orientation changes will be deferred until the drag ends.
  handle.SetOrientation(TOUCH_HANDLE_LEFT);
  EXPECT_EQ(TOUCH_HANDLE_RIGHT, drawable().orientation);

  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetHandleDragged());
  EXPECT_TRUE(IsDragging());
  EXPECT_EQ(TOUCH_HANDLE_RIGHT, drawable().orientation);

  event = MockMotionEvent(MockMotionEvent::ACTION_UP);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleDragged());
  EXPECT_FALSE(IsDragging());
  EXPECT_EQ(TOUCH_HANDLE_LEFT, drawable().orientation);
}

TEST_F(TouchHandleTest, DragDefersFade) {
  TouchHandle handle(this, TOUCH_HANDLE_CENTER);
  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);

  MockMotionEvent event(MockMotionEvent::ACTION_DOWN);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  // Fade will be deferred until the drag ends.
  handle.SetVisible(false, TouchHandle::ANIMATION_SMOOTH);
  EXPECT_FALSE(NeedsAnimate());
  EXPECT_TRUE(drawable().visible);
  EXPECT_EQ(1.f, drawable().alpha);

  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(NeedsAnimate());
  EXPECT_TRUE(drawable().visible);

  event = MockMotionEvent(MockMotionEvent::ACTION_UP);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());
  EXPECT_TRUE(NeedsAnimate());

  Animate(handle);
  EXPECT_FALSE(drawable().visible);
  EXPECT_EQ(0.f, drawable().alpha);
}

TEST_F(TouchHandleTest, DragTargettingUsesTouchSize) {
  TouchHandle handle(this, TOUCH_HANDLE_CENTER);
  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);

  base::TimeTicks event_time = base::TimeTicks::Now();
  const float kTouchSize = 24.f;
  const float kOffset = kDefaultDrawableSize + kTouchSize / 2.001f;

  MockMotionEvent event(
      MockMotionEvent::ACTION_DOWN, event_time, kOffset, kOffset);
  event.SetTouchMajor(0.f);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  event.SetTouchMajor(kTouchSize / 2.f);
  EXPECT_FALSE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(IsDragging());

  event.SetTouchMajor(kTouchSize);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  event.SetTouchMajor(kTouchSize * 2.f);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());

  // Ensure a touch size of 0 can still register a hit.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN,
                          event_time,
                          kDefaultDrawableSize / 2.f,
                          kDefaultDrawableSize / 2.f);
  event.SetTouchMajor(0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(IsDragging());
}

TEST_F(TouchHandleTest, Tap) {
  TouchHandle handle(this, TOUCH_HANDLE_CENTER);
  handle.SetVisible(true, TouchHandle::ANIMATION_NONE);

  base::TimeTicks event_time = base::TimeTicks::Now();

  // ACTION_CANCEL shouldn't trigger a tap.
  MockMotionEvent event(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event_time += base::TimeDelta::FromMilliseconds(50);
  event = MockMotionEvent(MockMotionEvent::ACTION_CANCEL, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleTapped());

  // Long press shouldn't trigger a tap.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event_time += base::TimeDelta::FromMilliseconds(500);
  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetHandleTapped());

  // Only a brief tap should trigger a tap.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  event_time += base::TimeDelta::FromMilliseconds(50);
  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 0, 0);
  EXPECT_TRUE(handle.WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetHandleTapped());
}

}  // namespace content
