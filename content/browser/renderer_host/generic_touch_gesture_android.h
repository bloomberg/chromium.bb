// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_GENERIC_TOUCH_GESTURE_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_GENERIC_TOUCH_GESTURE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/synthetic_gesture_calculator.h"
#include "content/port/browser/synthetic_gesture.h"

namespace content {

class ContentViewCore;
class RenderWidgetHost;

class GenericTouchGestureAndroid : public SyntheticGesture {
 public:
  GenericTouchGestureAndroid(
      RenderWidgetHost* rwh,
      base::android::ScopedJavaLocalRef<jobject> java_scroller);

  // Called by the java side once the TimeAnimator ticks.
  float GetDelta(JNIEnv* env, jobject obj, float scale);
  void SetHasFinished(JNIEnv* env, jobject obj);

  // SmoothScrollGesture
  virtual bool ForwardInputEvents(base::TimeTicks now,
                                  RenderWidgetHost* host) OVERRIDE;

 private:
  virtual ~GenericTouchGestureAndroid();

  SyntheticGestureCalculator synthetic_gesture_calculator_;

  bool has_started_;
  bool has_finished_;

  RenderWidgetHost* rwh_;
  base::android::ScopedJavaGlobalRef<jobject> java_touch_gesture_;

  DISALLOW_COPY_AND_ASSIGN(GenericTouchGestureAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_GENERIC_TOUCH_GESTURE_ANDROID_H_
