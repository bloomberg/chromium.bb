// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/gesture_text_selector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/events/test/mock_motion_event.h"
#include "ui/gfx/geometry/rect_f.h"

using ui::MotionEvent;
using ui::test::MockMotionEvent;

namespace content {

class GestureTextSelectorTest : public testing::Test,
                                public GestureTextSelectorClient {
 public:
  GestureTextSelectorTest() {}
  virtual ~GestureTextSelectorTest() {}

  // Test implementation.
  virtual void SetUp() OVERRIDE {
    selector_.reset(new GestureTextSelector(this));
    event_log_.clear();
  }

  virtual void TearDown() OVERRIDE {
    selector_.reset();
    event_log_.clear();
  }

  // GestureTextSelectorClient implementation.
  virtual void ShowSelectionHandlesAutomatically() OVERRIDE {
    event_log_.push_back("Show");
  }

  virtual void SelectRange(float x1, float y1, float x2, float y2) OVERRIDE {
    event_log_.push_back("SelectRange");
  }

  virtual void LongPress(base::TimeTicks time, float x, float y) OVERRIDE {
    event_log_.push_back("LongPress");
  }

 protected:
  scoped_ptr<GestureTextSelector> selector_;
  std::vector<std::string> event_log_;
};

TEST_F(GestureTextSelectorTest, ShouldStartTextSelection) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  {  // Touched with a finger.
    MockMotionEvent e(MotionEvent::ACTION_DOWN, event_time, 50.0f, 50.0f);
    e.SetToolType(0, MotionEvent::TOOL_TYPE_FINGER);
    e.set_button_state(0);
    EXPECT_FALSE(selector_->ShouldStartTextSelection(e));
  }

  {  // Touched with a stylus, but no button pressed.
    MockMotionEvent e(MotionEvent::ACTION_DOWN, event_time, 50.0f, 50.0f);
    e.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
    e.set_button_state(0);
    EXPECT_FALSE(selector_->ShouldStartTextSelection(e));
  }

  {  // Touched with a stylus, with first button (BUTTON_SECONDARY) pressed.
    MockMotionEvent e(MotionEvent::ACTION_DOWN, event_time, 50.0f, 50.0f);
    e.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
    e.set_button_state(MotionEvent::BUTTON_SECONDARY);
    EXPECT_TRUE(selector_->ShouldStartTextSelection(e));
  }

  {  // Touched with a stylus, with two buttons pressed.
    MockMotionEvent e(MotionEvent::ACTION_DOWN, event_time, 50.0f, 50.0f);
    e.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
    e.set_button_state(
        MotionEvent::BUTTON_SECONDARY | MotionEvent::BUTTON_TERTIARY);
    EXPECT_FALSE(selector_->ShouldStartTextSelection(e));
  }
}

TEST_F(GestureTextSelectorTest, FingerTouch) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  const float x = 50.0f;
  const float y = 30.0f;
  // 1. Touched with a finger: ignored
  MockMotionEvent finger(MotionEvent::ACTION_DOWN, event_time, x, y);
  finger.SetToolType(0, MotionEvent::TOOL_TYPE_FINGER);
  EXPECT_FALSE(selector_->OnTouchEvent(finger));
  // We do not consume finger events.
  EXPECT_TRUE(event_log_.empty());
}

TEST_F(GestureTextSelectorTest, PenDragging) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  const float x1 = 50.0f;
  const float y1 = 30.0f;
  const float x2 = 100.0f;
  const float y2 = 90.0f;
  // 1. ACTION_DOWN with stylus + button
  event_time += base::TimeDelta::FromMilliseconds(10);
  MockMotionEvent action_down(MotionEvent::ACTION_DOWN, event_time, x1, y1);
  action_down.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
  action_down.set_button_state(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_down));
  EXPECT_TRUE(event_log_.empty());

  // 2. ACTION_MOVE
  event_time += base::TimeDelta::FromMilliseconds(10);
  MockMotionEvent action_move(MotionEvent::ACTION_MOVE, event_time, x2, y2);
  action_move.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
  action_move.set_button_state(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  ASSERT_EQ(2u, event_log_.size());
  EXPECT_STREQ("Show", event_log_[0].c_str());
  EXPECT_STREQ("SelectRange", event_log_[1].c_str());

  // 3. ACTION_UP
  event_time += base::TimeDelta::FromMilliseconds(10);
  MockMotionEvent action_up(MotionEvent::ACTION_UP, event_time, x2, y2);
  action_up.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
  action_up.set_button_state(0);
  EXPECT_TRUE(selector_->OnTouchEvent(action_up));
  ASSERT_EQ(2u, event_log_.size());  // NO CHANGE
}

TEST_F(GestureTextSelectorTest, TapTriggersLongPressSelection) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  const float x1 = 50.0f;
  const float y1 = 30.0f;
  const float x2 = 51.0f;
  const float y2 = 31.0f;
  // 1. ACTION_DOWN with stylus + button
  event_time += base::TimeDelta::FromMilliseconds(1);
  MockMotionEvent action_down(MotionEvent::ACTION_DOWN, event_time, x1, y1);
  action_down.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
  action_down.set_button_state(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_down));
  EXPECT_TRUE(event_log_.empty());

  // 2. ACTION_MOVE
  event_time += base::TimeDelta::FromMilliseconds(1);
  MockMotionEvent action_move(MotionEvent::ACTION_MOVE, event_time, x2, y2);
  action_move.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
  action_move.set_button_state(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  EXPECT_TRUE(event_log_.empty());

  // 3. ACTION_UP
  event_time += base::TimeDelta::FromMilliseconds(1);
  MockMotionEvent action_up(MotionEvent::ACTION_UP, event_time, x2, y2);
  action_up.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
  action_up.set_button_state(0);
  EXPECT_TRUE(selector_->OnTouchEvent(action_up));
  ASSERT_EQ(1u, event_log_.size());
  EXPECT_STREQ("LongPress", event_log_.back().c_str());
}

}  // namespace content
