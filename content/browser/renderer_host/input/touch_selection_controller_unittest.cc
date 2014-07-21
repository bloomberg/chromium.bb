// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_selection_controller.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/mock_motion_event.h"

using ui::test::MockMotionEvent;

namespace content {
namespace {

class MockTouchHandleDrawable : public TouchHandleDrawable {
 public:
  explicit MockTouchHandleDrawable(bool* contains_point)
      : contains_point_(contains_point) {}
  virtual ~MockTouchHandleDrawable() {}
  virtual void SetEnabled(bool enabled) OVERRIDE {}
  virtual void SetOrientation(TouchHandleOrientation orientation) OVERRIDE {}
  virtual void SetAlpha(float alpha) OVERRIDE {}
  virtual void SetFocus(const gfx::PointF& position) OVERRIDE {}
  virtual void SetVisible(bool visible) OVERRIDE {}
  virtual bool ContainsPoint(const gfx::PointF& point) const OVERRIDE {
    return *contains_point_;
  }

 private:
  bool* contains_point_;
};

}  // namespace

class TouchSelectionControllerTest : public testing::Test,
                                     public TouchSelectionControllerClient {
 public:
  TouchSelectionControllerTest()
      : last_event_(SELECTION_CLEARED),
        caret_moved_(false),
        selection_moved_(false),
        needs_animate_(false),
        animation_enabled_(true),
        dragging_enabled_(false) {}

  virtual ~TouchSelectionControllerTest() {}

  // testing::Test implementation.
  virtual void SetUp() OVERRIDE {
    controller_.reset(new TouchSelectionController(this));
  }

  virtual void TearDown() OVERRIDE { controller_.reset(); }

  // TouchSelectionControllerClient implementation.

  virtual bool SupportsAnimation() const OVERRIDE { return animation_enabled_; }

  virtual void SetNeedsAnimate() OVERRIDE { needs_animate_ = true; }

  virtual void MoveCaret(const gfx::PointF& position) OVERRIDE {
    caret_moved_ = true;
    caret_position_ = position;
  }

  virtual void SelectBetweenCoordinates(const gfx::PointF& start,
                                        const gfx::PointF& end) OVERRIDE {
    selection_moved_ = true;
    selection_start_ = start;
    selection_end_ = end;
  }

  virtual void OnSelectionEvent(SelectionEventType event,
                                const gfx::PointF& anchor_position) OVERRIDE {
    last_event_ = event;
    last_event_anchor_ = anchor_position;
  }

  virtual scoped_ptr<TouchHandleDrawable> CreateDrawable() OVERRIDE {
    return scoped_ptr<TouchHandleDrawable>(
        new MockTouchHandleDrawable(&dragging_enabled_));
  }

  void SetAnimationEnabled(bool enabled) { animation_enabled_ = enabled; }
  void SetDraggingEnabled(bool enabled) { dragging_enabled_ = enabled; }

  void ClearSelection() {
    controller_->OnSelectionBoundsChanged(gfx::RectF(),
                                          TOUCH_HANDLE_ORIENTATION_UNDEFINED,
                                          false,
                                          gfx::RectF(),
                                          TOUCH_HANDLE_ORIENTATION_UNDEFINED,
                                          false);
  }

  void ClearInsertion() { ClearSelection(); }

  void ChangeInsertion(const gfx::RectF& rect,
                       TouchHandleOrientation orientation,
                       bool visible) {
    controller_->OnSelectionBoundsChanged(
        rect, orientation, visible, rect, orientation, visible);
  }

  void ChangeInsertion(const gfx::RectF& rect, bool visible) {
    ChangeInsertion(rect, TOUCH_HANDLE_CENTER, visible);
  }

  void ChangeSelection(const gfx::RectF& anchor_rect,
                       TouchHandleOrientation anchor_orientation,
                       bool anchor_visible,
                       const gfx::RectF& focus_rect,
                       TouchHandleOrientation focus_orientation,
                       bool focus_visible) {
    controller_->OnSelectionBoundsChanged(anchor_rect,
                                          anchor_orientation,
                                          anchor_visible,
                                          focus_rect,
                                          focus_orientation,
                                          focus_visible);
  }

  void ChangeSelection(const gfx::RectF& anchor_rect,
                       bool anchor_visible,
                       const gfx::RectF& focus_rect,
                       bool focus_visible) {
    ChangeSelection(anchor_rect,
                    TOUCH_HANDLE_LEFT,
                    anchor_visible,
                    focus_rect,
                    TOUCH_HANDLE_RIGHT,
                    focus_visible);
  }

  void Animate() {
    base::TimeTicks now = base::TimeTicks::Now();
    while (needs_animate_) {
      needs_animate_ = controller_->Animate(now);
      now += base::TimeDelta::FromMilliseconds(16);
    }
  }

  bool GetAndResetNeedsAnimate() {
    bool needs_animate = needs_animate_;
    Animate();
    return needs_animate;
  }

  bool GetAndResetCaretMoved() {
    bool moved = caret_moved_;
    caret_moved_ = false;
    return moved;
  }

  bool GetAndResetSelectionMoved() {
    bool moved = selection_moved_;
    selection_moved_ = false;
    return moved;
  }

  const gfx::PointF& GetLastCaretPosition() const { return caret_position_; }
  const gfx::PointF& GetLastSelectionStart() const { return selection_start_; }
  const gfx::PointF& GetLastSelectionEnd() const { return selection_end_; }
  SelectionEventType GetLastEventType() const { return last_event_; }
  const gfx::PointF& GetLastEventAnchor() const { return last_event_anchor_; }

  TouchSelectionController& controller() { return *controller_; }

 private:
  gfx::PointF last_event_anchor_;
  gfx::PointF caret_position_;
  gfx::PointF selection_start_;
  gfx::PointF selection_end_;
  SelectionEventType last_event_;
  bool caret_moved_;
  bool selection_moved_;
  bool needs_animate_;
  bool animation_enabled_;
  bool dragging_enabled_;
  scoped_ptr<TouchSelectionController> controller_;
};

TEST_F(TouchSelectionControllerTest, InsertionBasic) {
  gfx::RectF insertion_rect(5, 5, 0, 10);
  bool visible = true;

  // Insertion events are ignored until automatic showing is enabled.
  ChangeInsertion(insertion_rect, visible);
  EXPECT_EQ(gfx::PointF(), GetLastEventAnchor());
  controller().ShowInsertionHandleAutomatically();

  // Insertion events are ignored until the selection region is marked editable.
  ChangeInsertion(insertion_rect, visible);
  EXPECT_EQ(gfx::PointF(), GetLastEventAnchor());
  controller().OnSelectionEditable(true);

  ChangeInsertion(insertion_rect, visible);
  EXPECT_EQ(INSERTION_SHOWN, GetLastEventType());
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventAnchor());

  insertion_rect.Offset(1, 0);
  ChangeInsertion(insertion_rect, visible);
  EXPECT_EQ(INSERTION_MOVED, GetLastEventType());
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventAnchor());

  insertion_rect.Offset(0, 1);
  ChangeInsertion(insertion_rect, visible);
  EXPECT_EQ(INSERTION_MOVED, GetLastEventType());
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventAnchor());

  ClearInsertion();
  EXPECT_EQ(INSERTION_CLEARED, GetLastEventType());
}

TEST_F(TouchSelectionControllerTest, InsertionClearedWhenNoLongerEditable) {
  gfx::RectF insertion_rect(5, 5, 0, 10);
  bool visible = true;
  controller().ShowInsertionHandleAutomatically();
  controller().OnSelectionEditable(true);

  ChangeInsertion(insertion_rect, visible);
  EXPECT_EQ(INSERTION_SHOWN, GetLastEventType());
  EXPECT_EQ(insertion_rect.bottom_left(), GetLastEventAnchor());

  controller().OnSelectionEditable(false);
  EXPECT_EQ(INSERTION_CLEARED, GetLastEventType());
}

TEST_F(TouchSelectionControllerTest, InsertionToSelectionTransition) {
  controller().ShowSelectionHandlesAutomatically();
  controller().ShowInsertionHandleAutomatically();
  controller().OnSelectionEditable(true);

  gfx::RectF anchor_rect(5, 5, 0, 10);
  gfx::RectF focus_rect(50, 5, 0, 10);
  bool visible = true;

  ChangeInsertion(anchor_rect, visible);
  EXPECT_EQ(INSERTION_SHOWN, GetLastEventType());
  EXPECT_EQ(anchor_rect.bottom_left(), GetLastEventAnchor());

  ChangeSelection(anchor_rect, visible, focus_rect, visible);
  EXPECT_EQ(SELECTION_SHOWN, GetLastEventType());
  EXPECT_EQ(anchor_rect.bottom_left(), GetLastEventAnchor());

  ChangeInsertion(focus_rect, visible);
  EXPECT_EQ(INSERTION_SHOWN, GetLastEventType());
  EXPECT_EQ(focus_rect.bottom_left(), GetLastEventAnchor());

  controller().HideAndDisallowAutomaticShowing();
  EXPECT_EQ(INSERTION_CLEARED, GetLastEventType());

  controller().ShowInsertionHandleAutomatically();
  ChangeInsertion(focus_rect, visible);
  EXPECT_EQ(INSERTION_SHOWN, GetLastEventType());
  EXPECT_EQ(focus_rect.bottom_left(), GetLastEventAnchor());
}

TEST_F(TouchSelectionControllerTest, InsertionDragged) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  controller().ShowInsertionHandleAutomatically();
  controller().OnSelectionEditable(true);

  // The touch sequence should not be handled if insertion is not active.
  MockMotionEvent event(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));

  float line_height = 10.f;
  gfx::RectF anchor_rect(10, 0, 0, line_height);
  bool visible = true;
  ChangeInsertion(anchor_rect, visible);
  EXPECT_EQ(INSERTION_SHOWN, GetLastEventType());
  EXPECT_EQ(anchor_rect.bottom_left(), GetLastEventAnchor());

  // The touch sequence should be handled only if the drawable reports a hit.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));
  SetDraggingEnabled(true);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetCaretMoved());

  // The MoveCaret() result should reflect the movement.
  // The reported position is offset from the center of |anchor_rect|.
  gfx::PointF anchor_offset = anchor_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time, 0, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetCaretMoved());
  EXPECT_EQ(anchor_offset + gfx::Vector2dF(0, 5), GetLastCaretPosition());

  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time, 5, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetCaretMoved());
  EXPECT_EQ(anchor_offset + gfx::Vector2dF(5, 5), GetLastCaretPosition());

  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time, 10, 10);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetCaretMoved());
  EXPECT_EQ(anchor_offset + gfx::Vector2dF(10, 10), GetLastCaretPosition());

  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetCaretMoved());

  // Once the drag is complete, no more touch events should be consumed until
  // the next ACTION_DOWN.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));
}

TEST_F(TouchSelectionControllerTest, InsertionTapped) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  controller().ShowInsertionHandleAutomatically();
  controller().OnSelectionEditable(true);
  SetDraggingEnabled(true);

  gfx::RectF anchor_rect(10, 0, 0, 10);
  bool visible = true;
  ChangeInsertion(anchor_rect, visible);
  EXPECT_EQ(INSERTION_SHOWN, GetLastEventType());

  MockMotionEvent event(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(INSERTION_SHOWN, GetLastEventType());

  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(INSERTION_TAPPED, GetLastEventType());

  // Reset the insertion.
  ClearInsertion();
  controller().ShowInsertionHandleAutomatically();
  ChangeInsertion(anchor_rect, visible);
  ASSERT_EQ(INSERTION_SHOWN, GetLastEventType());

  // No tap should be signalled if the time between DOWN and UP was too long.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::ACTION_UP,
                          event_time + base::TimeDelta::FromSeconds(1),
                          0,
                          0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(INSERTION_SHOWN, GetLastEventType());

  // Reset the insertion.
  ClearInsertion();
  controller().ShowInsertionHandleAutomatically();
  ChangeInsertion(anchor_rect, visible);
  ASSERT_EQ(INSERTION_SHOWN, GetLastEventType());

  // No tap should be signalled if the touch sequence is cancelled.
  event = MockMotionEvent(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  event = MockMotionEvent(MockMotionEvent::ACTION_CANCEL, event_time, 0, 0);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_EQ(INSERTION_SHOWN, GetLastEventType());
}

TEST_F(TouchSelectionControllerTest, SelectionBasic) {
  gfx::RectF anchor_rect(5, 5, 0, 10);
  gfx::RectF focus_rect(50, 5, 0, 10);
  bool visible = true;

  // Selection events are ignored until automatic showing is enabled.
  ChangeSelection(anchor_rect, visible, focus_rect, visible);
  EXPECT_EQ(gfx::PointF(), GetLastEventAnchor());

  controller().ShowSelectionHandlesAutomatically();
  ChangeSelection(anchor_rect, visible, focus_rect, visible);
  EXPECT_EQ(SELECTION_SHOWN, GetLastEventType());
  EXPECT_EQ(anchor_rect.bottom_left(), GetLastEventAnchor());

  anchor_rect.Offset(1, 0);
  ChangeSelection(anchor_rect, visible, focus_rect, visible);
  // Selection movement does not currently trigger a separate event.
  EXPECT_EQ(SELECTION_SHOWN, GetLastEventType());

  ClearSelection();
  EXPECT_EQ(SELECTION_CLEARED, GetLastEventType());
  EXPECT_EQ(gfx::PointF(), GetLastEventAnchor());
}

TEST_F(TouchSelectionControllerTest, SelectionDragged) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  controller().ShowSelectionHandlesAutomatically();

  // The touch sequence should not be handled if selection is not active.
  MockMotionEvent event(MockMotionEvent::ACTION_DOWN, event_time, 0, 0);
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));

  float line_height = 10.f;
  gfx::RectF anchor_rect(0, 0, 0, line_height);
  gfx::RectF focus_rect(50, 0, 0, line_height);
  bool visible = true;
  ChangeSelection(anchor_rect, visible, focus_rect, visible);
  EXPECT_EQ(SELECTION_SHOWN, GetLastEventType());
  EXPECT_EQ(anchor_rect.bottom_left(), GetLastEventAnchor());

  // The touch sequence should be handled only if the drawable reports a hit.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));
  SetDraggingEnabled(true);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetSelectionMoved());

  // The SelectBetweenCoordinates() result should reflect the movement. Note
  // that the start coordinate will always reflect the "fixed" handle's
  // position, in this case the position from |focus_rect|.
  // Note that the reported position is offset from the center of the
  // input rects (i.e., the middle of the corresponding text line).
  gfx::PointF fixed_offset = focus_rect.CenterPoint();
  gfx::PointF anchor_offset = anchor_rect.CenterPoint();
  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time, 0, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(fixed_offset, GetLastSelectionStart());
  EXPECT_EQ(anchor_offset + gfx::Vector2dF(0, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time, 5, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(fixed_offset, GetLastSelectionStart());
  EXPECT_EQ(anchor_offset + gfx::Vector2dF(5, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::ACTION_MOVE, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_TRUE(GetAndResetSelectionMoved());
  EXPECT_EQ(fixed_offset, GetLastSelectionStart());
  EXPECT_EQ(anchor_offset + gfx::Vector2dF(10, 5), GetLastSelectionEnd());

  event = MockMotionEvent(MockMotionEvent::ACTION_UP, event_time, 10, 5);
  EXPECT_TRUE(controller().WillHandleTouchEvent(event));
  EXPECT_FALSE(GetAndResetSelectionMoved());

  // Once the drag is complete, no more touch events should be consumed until
  // the next ACTION_DOWN.
  EXPECT_FALSE(controller().WillHandleTouchEvent(event));
}

TEST_F(TouchSelectionControllerTest, Animation) {
  controller().ShowInsertionHandleAutomatically();
  controller().OnSelectionEditable(true);

  gfx::RectF insertion_rect(5, 5, 0, 10);

  bool visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());

  visible = false;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_TRUE(GetAndResetNeedsAnimate());

  visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_TRUE(GetAndResetNeedsAnimate());

  // If the handles are explicity hidden, no animation should be triggered.
  controller().HideAndDisallowAutomaticShowing();
  EXPECT_FALSE(GetAndResetNeedsAnimate());

  // If the client doesn't support animation, no animation should be triggered.
  SetAnimationEnabled(false);
  controller().ShowInsertionHandleAutomatically();
  visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());
}

TEST_F(TouchSelectionControllerTest, TemporarilyHidden) {
  controller().ShowInsertionHandleAutomatically();
  controller().OnSelectionEditable(true);

  gfx::RectF insertion_rect(5, 5, 0, 10);

  bool visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());

  controller().SetTemporarilyHidden(true);
  EXPECT_TRUE(GetAndResetNeedsAnimate());

  visible = false;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());

  visible = true;
  ChangeInsertion(insertion_rect, visible);
  EXPECT_FALSE(GetAndResetNeedsAnimate());

  controller().SetTemporarilyHidden(false);
  EXPECT_TRUE(GetAndResetNeedsAnimate());
}

}  // namespace content
