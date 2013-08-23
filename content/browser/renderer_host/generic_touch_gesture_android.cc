// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/generic_touch_gesture_android.h"

#include "base/debug/trace_event.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "jni/GenericTouchGesture_jni.h"

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

GenericTouchGestureAndroid::GenericTouchGestureAndroid(
    RenderWidgetHost* rwh,
    base::android::ScopedJavaLocalRef<jobject> java_touch_gesture)
    : has_started_(false),
      has_finished_(false),
      rwh_(rwh),
      java_touch_gesture_(java_touch_gesture) {
  JNIEnv* env = base::android::AttachCurrentThread();
  RegisterNativesIfNeeded(env);
}

GenericTouchGestureAndroid::~GenericTouchGestureAndroid() {
}

bool GenericTouchGestureAndroid::ForwardInputEvents(
    base::TimeTicks now, RenderWidgetHost* host) {
  if (!has_started_) {
    has_started_ = true;
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_GenericTouchGesture_start(
        env, java_touch_gesture_.obj(), reinterpret_cast<intptr_t>(this));
  }

  return !has_finished_;
}

float GenericTouchGestureAndroid::GetDelta(
    JNIEnv* env, jobject obj, float scale) {
  return synthetic_gesture_calculator_.GetDelta(
      base::TimeTicks::Now(),
      RenderWidgetHostImpl::From(rwh_)->GetSyntheticGestureMessageInterval()) *
      scale;
}

void GenericTouchGestureAndroid::SetHasFinished(JNIEnv* env, jobject obj) {
  has_finished_ = true;
}

}  // namespace content
