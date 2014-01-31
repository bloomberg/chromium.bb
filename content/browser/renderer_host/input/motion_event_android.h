// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"

namespace content {

// Utility wrapper class for Android's android.view.MotionEvent.
class MotionEventAndroid {
 public:
  // Note: These constants are taken directly from android.view.MotionEvent.
  //       Do NOT under any circumstances change these values.
  enum Action {
    ACTION_DOWN             = 0,
    ACTION_UP               = 1,
    ACTION_MOVE             = 2,
    ACTION_CANCEL           = 3,
    // Purposefully removed, as there is no analogous action in Chromium.
    // ACTION_OUTSIDE       = 4,
    ACTION_POINTER_DOWN     = 5,
    ACTION_POINTER_UP       = 6
  };

  explicit MotionEventAndroid(jobject event);
  ~MotionEventAndroid();

  Action GetActionMasked() const;
  size_t GetActionIndex() const;
  size_t GetPointerCount() const;
  int GetPointerId(size_t pointer_index) const;
  float GetPressure() const { return GetPressure(0); }
  float GetPressure(size_t pointer_index) const;
  float GetX() const { return GetX(0); }
  float GetY() const { return GetY(0); }
  float GetX(size_t pointer_index) const;
  float GetY(size_t pointer_index) const;
  float GetRawX() const { return GetX(); }
  float GetRawY() const { return GetY(); }
  float GetTouchMajor() const { return GetTouchMajor(0); }
  float GetTouchMajor(size_t pointer_index) const;
  float GetTouchMinor() const { return GetTouchMinor(0); }
  float GetTouchMinor(size_t pointer_index) const;
  float GetOrientation() const;
  base::TimeTicks GetEventTime() const;
  base::TimeTicks GetDownTime() const;

  size_t GetHistorySize() const;
  base::TimeTicks GetHistoricalEventTime(size_t historical_index) const;
  float GetHistoricalTouchMajor(size_t pointer_index,
                                size_t historical_index) const;
  float GetHistoricalX(size_t pointer_index, size_t historical_index) const;
  float GetHistoricalY(size_t pointer_index, size_t historical_index) const;

  static bool RegisterMotionEventAndroid(JNIEnv* env);

  static scoped_ptr<MotionEventAndroid> Obtain(const MotionEventAndroid& event);
  static scoped_ptr<MotionEventAndroid> Obtain(base::TimeTicks down_time,
                                               base::TimeTicks event_time,
                                               Action action,
                                               float x,
                                               float y);

 private:
  MotionEventAndroid(const base::android::ScopedJavaLocalRef<jobject>& event,
                     bool should_recycle);

  // The Java reference to the underlying MotionEvent.
  base::android::ScopedJavaGlobalRef<jobject> event_;

  // Whether |event_| should be recycled on destruction. This will only be true
  // for those events generated via |Obtain(...)|.
  bool should_recycle_;

  DISALLOW_COPY_AND_ASSIGN(MotionEventAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_ANDROID_H_
