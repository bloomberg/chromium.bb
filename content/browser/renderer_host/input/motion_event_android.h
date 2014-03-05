// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/events/gesture_detection/motion_event.h"

namespace content {

// Implementation of ui::MotionEvent wrapping a native Android MotionEvent.
class MotionEventAndroid : public ui::MotionEvent {
 public:
  MotionEventAndroid(JNIEnv* env, jobject event, bool recycle);
  virtual ~MotionEventAndroid();

  // ui::MotionEvent methods.
  virtual Action GetAction() const OVERRIDE;
  virtual int GetActionIndex() const OVERRIDE;
  virtual size_t GetPointerCount() const OVERRIDE;
  virtual int GetPointerId(size_t pointer_index) const OVERRIDE;
  virtual float GetX(size_t pointer_index) const OVERRIDE;
  virtual float GetY(size_t pointer_index) const OVERRIDE;
  virtual float GetTouchMajor(size_t pointer_index) const OVERRIDE;
  virtual float GetPressure(size_t pointer_index) const OVERRIDE;
  virtual base::TimeTicks GetEventTime() const OVERRIDE;
  virtual size_t GetHistorySize() const OVERRIDE;
  virtual base::TimeTicks GetHistoricalEventTime(
      size_t historical_index) const OVERRIDE;
  virtual float GetHistoricalTouchMajor(
      size_t pointer_index,
      size_t historical_index) const OVERRIDE;
  virtual float GetHistoricalX(
      size_t pointer_index,
      size_t historical_index) const OVERRIDE;
  virtual float GetHistoricalY(
      size_t pointer_index,
      size_t historical_index) const OVERRIDE;
  virtual scoped_ptr<MotionEvent> Clone() const OVERRIDE;
  virtual scoped_ptr<MotionEvent> Cancel() const OVERRIDE;

  // Additional Android MotionEvent methods.
  float GetTouchMinor() const { return GetTouchMinor(0); }
  float GetTouchMinor(size_t pointer_index) const;
  float GetOrientation() const;
  base::TimeTicks GetDownTime() const;

  static bool RegisterMotionEventAndroid(JNIEnv* env);

  static base::android::ScopedJavaLocalRef<jobject> Obtain(
      const MotionEventAndroid& event);
  static base::android::ScopedJavaLocalRef<jobject> Obtain(
      base::TimeTicks down_time,
      base::TimeTicks event_time,
      Action action,
      float x,
      float y);

 private:
  MotionEventAndroid();
  MotionEventAndroid(const MotionEventAndroid&);
  MotionEventAndroid& operator=(const MotionEventAndroid&);

  // The Java reference to the underlying MotionEvent.
  base::android::ScopedJavaGlobalRef<jobject> event_;

  base::TimeTicks cached_time_;
  Action cached_action_;
  size_t cached_pointer_count_;
  size_t cached_history_size_;
  int cached_action_index_;
  float cached_x_;
  float cached_y_;

  // Whether |event_| should be recycled on destruction. This will only be true
  // for those events generated via |Obtain(...)|.
  bool should_recycle_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_ANDROID_H_
