// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "content/browser/renderer_host/input/motion_event_android.h"
#include "testing/gtest/include/gtest/gtest.h"

using ui::MotionEvent;

namespace content {
namespace {
const float kPixToDip = 0.5f;

// Corresponds to ACTION_DOWN, see
// developer.android.com/reference/android/view/MotionEvent.html#ACTION_DOWN.
int kAndroidActionDown = 0;

// Corresponds to TOOL_TYPE_FINGER, see
// developer.android.com/reference/android/view/MotionEvent.html
//     #TOOL_TYPE_FINGER.
int kAndroidToolTypeFinger = 1;

// Corresponds to BUTTON_PRIMARY, see
// developer.android.com/reference/android/view/MotionEvent.html#BUTTON_PRIMARY.
int kAndroidButtonPrimary = 1;

}  // namespace

TEST(MotionEventAndroidTest, Constructor) {
  int event_time_ms = 5;
  base::TimeTicks event_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(event_time_ms);
  float x0 = 13.7;
  float y0 = -7.13;
  float x1 = -13.7;
  float y1 = 7.13;
  float raw_offset = 10.1f;
  float touch_major0 = 5.3f;
  float touch_major1 = 3.5f;
  int p0 = 1;
  int p1 = 2;
  int pointer_count = 2;
  int history_size = 0;
  int action_index = -1;
  base::android::ScopedJavaLocalRef<jobject> base_event_obj =
      MotionEventAndroid::Obtain(
          event_time, event_time, MotionEvent::ACTION_DOWN, x0, y0);
  ASSERT_TRUE(base_event_obj.obj());

  MotionEventAndroid event(kPixToDip,
                           base::android::AttachCurrentThread(),
                           base_event_obj.obj(),
                           event_time_ms,
                           kAndroidActionDown,
                           pointer_count,
                           history_size,
                           action_index,
                           x0,
                           y0,
                           x1,
                           y1,
                           p0,
                           p1,
                           touch_major0,
                           touch_major1,
                           x0 + raw_offset,
                           y0 - raw_offset,
                           kAndroidToolTypeFinger,
                           kAndroidToolTypeFinger,
                           kAndroidButtonPrimary);

  EXPECT_EQ(MotionEvent::ACTION_DOWN, event.GetAction());
  EXPECT_EQ(event_time, event.GetEventTime());
  EXPECT_EQ(x0 * kPixToDip, event.GetX(0));
  EXPECT_EQ(y0 * kPixToDip, event.GetY(0));
  EXPECT_EQ(x1 * kPixToDip, event.GetX(1));
  EXPECT_EQ(y1 * kPixToDip, event.GetY(1));
  EXPECT_FLOAT_EQ((x0 + raw_offset) * kPixToDip, event.GetRawX(0));
  EXPECT_FLOAT_EQ((y0 - raw_offset) * kPixToDip, event.GetRawY(0));
  EXPECT_FLOAT_EQ((x1 + raw_offset) * kPixToDip, event.GetRawX(1));
  EXPECT_FLOAT_EQ((y1 - raw_offset) * kPixToDip, event.GetRawY(1));
  EXPECT_EQ(touch_major0 * kPixToDip, event.GetTouchMajor(0));
  EXPECT_EQ(touch_major1 * kPixToDip, event.GetTouchMajor(1));
  EXPECT_EQ(p0, event.GetPointerId(0));
  EXPECT_EQ(p1, event.GetPointerId(1));
  EXPECT_EQ(MotionEvent::TOOL_TYPE_FINGER, event.GetToolType(0));
  EXPECT_EQ(MotionEvent::TOOL_TYPE_FINGER, event.GetToolType(1));
  EXPECT_EQ(MotionEvent::BUTTON_PRIMARY, event.GetButtonState());
  EXPECT_EQ(static_cast<size_t>(pointer_count), event.GetPointerCount());
  EXPECT_EQ(static_cast<size_t>(history_size), event.GetHistorySize());
  EXPECT_EQ(action_index, event.GetActionIndex());
}

TEST(MotionEventAndroidTest, Clone) {
  int event_time_ms = 5;
  base::TimeTicks event_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(event_time_ms);
  float x = 13.7;
  float y = -7.13;
  float touch_major = 5.3f;
  int pointer_count = 1;
  int pointer_id = 1;
  base::android::ScopedJavaLocalRef<jobject> event_obj =
      MotionEventAndroid::Obtain(
          event_time, event_time, MotionEvent::ACTION_DOWN, x, y);
  ASSERT_TRUE(event_obj.obj());

  MotionEventAndroid event(kPixToDip,
                           base::android::AttachCurrentThread(),
                           event_obj.obj(),
                           event_time_ms,
                           kAndroidActionDown,
                           pointer_count,
                           0,
                           0,
                           x,
                           y,
                           0,
                           0,
                           pointer_id,
                           0,
                           touch_major,
                           0.f,
                           x,
                           y,
                           0,
                           0,
                           0);

  scoped_ptr<MotionEvent> clone = event.Clone();
  EXPECT_EQ(event, *clone);
}

TEST(MotionEventAndroidTest, Cancel) {
  int event_time_ms = 5;
  base::TimeTicks event_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(event_time_ms);
  int pointer_count = 1;
  float x = 13.7;
  float y = -7.13;
  base::android::ScopedJavaLocalRef<jobject> event_obj =
      MotionEventAndroid::Obtain(
          event_time, event_time, MotionEvent::ACTION_DOWN, x, y);
  ASSERT_TRUE(event_obj.obj());

  MotionEventAndroid event(kPixToDip,
                           base::android::AttachCurrentThread(),
                           event_obj.obj(),
                           event_time_ms,
                           kAndroidActionDown,
                           pointer_count,
                           0,
                           0,
                           x,
                           y,
                           0,
                           0,
                           0,
                           0,
                           0.f,
                           0.f,
                           x,
                           y,
                           0,
                           0,
                           0);

  scoped_ptr<MotionEvent> cancel_event = event.Cancel();
  EXPECT_EQ(MotionEvent::ACTION_CANCEL, cancel_event->GetAction());
  EXPECT_EQ(event_time, cancel_event->GetEventTime());
  EXPECT_EQ(x * kPixToDip, cancel_event->GetX(0));
  EXPECT_EQ(y * kPixToDip, cancel_event->GetY(0));
  EXPECT_EQ(static_cast<size_t>(pointer_count),
            cancel_event->GetPointerCount());
  EXPECT_EQ(0U, cancel_event->GetHistorySize());
}

}  // namespace content
