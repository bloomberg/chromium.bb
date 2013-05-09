// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/touch_smooth_scroll_gesture_android.h"

#include "base/debug/trace_event.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "jni/SmoothScroller_jni.h"

namespace {
bool g_jni_initialized = false;

void RegisterNativesIfNeeded(JNIEnv* env) {
  if (!g_jni_initialized) {
    content::RegisterNativesImpl(env);
    g_jni_initialized = true;
  }
}
}  // namespace

namespace content {

TouchSmoothScrollGestureAndroid::TouchSmoothScrollGestureAndroid(
    int pixels_to_scroll,
    RenderWidgetHost* rwh,
    base::android::ScopedJavaLocalRef<jobject> java_scroller)
    : pixels_scrolled_(0),
      has_started_(false),
      has_sent_motion_up_(false),
      pixels_to_scroll_(pixels_to_scroll),
      rwh_(rwh),
      java_scroller_(java_scroller) {
  JNIEnv* env = base::android::AttachCurrentThread();
  RegisterNativesIfNeeded(env);
}

TouchSmoothScrollGestureAndroid::~TouchSmoothScrollGestureAndroid() {
}

bool TouchSmoothScrollGestureAndroid::ForwardInputEvents(
    base::TimeTicks now, RenderWidgetHost* host) {
  if (!has_started_) {
    has_started_ = true;
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_SmoothScroller_start(
        env, java_scroller_.obj(), reinterpret_cast<int>(this));
  }

  TRACE_COUNTER_ID1(
      "gpu", "smooth_scroll_by_pixels_scrolled", this, pixels_scrolled_);

  return !has_sent_motion_up_;
}

double TouchSmoothScrollGestureAndroid::GetScrollDelta(
    JNIEnv* env, jobject obj, double scale) {
  double delta = smooth_scroll_calculator_.GetScrollDelta(
      base::TimeTicks::Now(),
      RenderWidgetHostImpl::From(rwh_)->GetSyntheticScrollMessageInterval())
      * scale;
  pixels_scrolled_ += delta;
  return delta;
}

bool TouchSmoothScrollGestureAndroid::HasFinished(JNIEnv* env, jobject obj) {
  return pixels_scrolled_ >= pixels_to_scroll_;
}

void TouchSmoothScrollGestureAndroid::SetHasSentMotionUp(
    JNIEnv* env, jobject obj) {
  has_sent_motion_up_ = true;
}

}  // namespace content
