// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SMOOTH_GESTURE_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_SMOOTH_GESTURE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/time.h"
#include "content/port/browser/smooth_scroll_gesture.h"

namespace content {

class ContentViewCore;
class RenderWidgetHost;

class SmoothScrollGestureAndroid : public SmoothScrollGesture {
 public:
  SmoothScrollGestureAndroid(
      int pixels_to_scroll,
      RenderWidgetHost* rwh,
      base::android::ScopedJavaLocalRef<jobject> java_scroller);

  // Called by the java side once the TimeAnimator ticks.
  double Tick(JNIEnv* env, jobject obj);
  bool HasFinished(JNIEnv* env, jobject obj);

  // SmoothScrollGesture
  virtual bool ForwardInputEvents(base::TimeTicks now,
                                  RenderWidgetHost* host) OVERRIDE;

 private:
  virtual ~SmoothScrollGestureAndroid();

  int pixels_scrolled_;
  bool has_started_;

  int pixels_to_scroll_;
  RenderWidgetHost* rwh_;
  base::android::ScopedJavaGlobalRef<jobject> java_scroller_;

  DISALLOW_COPY_AND_ASSIGN(SmoothScrollGestureAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SMOOTH_GESTURE_ANDROID_H_
