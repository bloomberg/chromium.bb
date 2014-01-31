// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/motion_event_android.h"

#include "base/android/jni_android.h"
#include "jni/MotionEvent_jni.h"

using base::android::AttachCurrentThread;
using namespace JNI_MotionEvent;

namespace content {
namespace {

MotionEventAndroid::Action FromAndroidAction(int android_action) {
  switch (android_action) {
    case MotionEventAndroid::ACTION_DOWN:
    case MotionEventAndroid::ACTION_UP:
    case MotionEventAndroid::ACTION_MOVE:
    case MotionEventAndroid::ACTION_CANCEL:
    case MotionEventAndroid::ACTION_POINTER_DOWN:
    case MotionEventAndroid::ACTION_POINTER_UP:
      return static_cast<MotionEventAndroid::Action>(android_action);
    default:
      NOTREACHED() << "Invalid Android MotionEvent type for gesture detection: "
                   << android_action;
  };
  return MotionEventAndroid::ACTION_CANCEL;
}

int64 ToAndroidTime(base::TimeTicks time) {
  return (time - base::TimeTicks()).InMilliseconds();
}

base::TimeTicks FromAndroidTime(int64 time_ms) {
  return base::TimeTicks() + base::TimeDelta::FromMilliseconds(time_ms);
}

}  // namespace

MotionEventAndroid::MotionEventAndroid(jobject event)
    : should_recycle_(false) {
  event_.Reset(AttachCurrentThread(), event);
  DCHECK(event_.obj());
}

MotionEventAndroid::MotionEventAndroid(
    const base::android::ScopedJavaLocalRef<jobject>& event,
    bool should_recycle)
    : event_(event),
      should_recycle_(should_recycle) {
  DCHECK(event_.obj());
}

MotionEventAndroid::~MotionEventAndroid() {
  if (should_recycle_)
    Java_MotionEvent_recycle(AttachCurrentThread(), event_.obj());
}

MotionEventAndroid::Action MotionEventAndroid::GetActionMasked() const {
  return FromAndroidAction(
      Java_MotionEvent_getActionMasked(AttachCurrentThread(), event_.obj()));
}

size_t MotionEventAndroid::GetActionIndex() const {
  return Java_MotionEvent_getActionIndex(AttachCurrentThread(), event_.obj());
}

size_t MotionEventAndroid::GetPointerCount() const {
  return Java_MotionEvent_getPointerCount(AttachCurrentThread(), event_.obj());
}

int MotionEventAndroid::GetPointerId(size_t pointer_index) const {
  return Java_MotionEvent_getPointerId(
      AttachCurrentThread(), event_.obj(), pointer_index);
}

float MotionEventAndroid::GetPressure(size_t pointer_index) const {
  return Java_MotionEvent_getPressureF_I(
      AttachCurrentThread(), event_.obj(), pointer_index);
}

float MotionEventAndroid::GetX(size_t pointer_index) const {
  return Java_MotionEvent_getXF_I(
      AttachCurrentThread(), event_.obj(), pointer_index);
}

float MotionEventAndroid::GetY(size_t pointer_index) const {
  return Java_MotionEvent_getYF_I(
      AttachCurrentThread(), event_.obj(), pointer_index);
}

float MotionEventAndroid::GetTouchMajor(size_t pointer_index) const {
  return Java_MotionEvent_getTouchMajorF_I(
      AttachCurrentThread(), event_.obj(), pointer_index);
}

float MotionEventAndroid::GetTouchMinor(size_t pointer_index) const {
  return Java_MotionEvent_getTouchMinorF_I(
      AttachCurrentThread(), event_.obj(), pointer_index);
}

float MotionEventAndroid::GetOrientation() const {
  return Java_MotionEvent_getOrientationF(AttachCurrentThread(), event_.obj());
}

base::TimeTicks MotionEventAndroid::GetEventTime() const {
  return FromAndroidTime(
      Java_MotionEvent_getEventTime(AttachCurrentThread(), event_.obj()));
}

base::TimeTicks MotionEventAndroid::GetDownTime() const {
  return FromAndroidTime(
      Java_MotionEvent_getDownTime(AttachCurrentThread(), event_.obj()));
}

size_t MotionEventAndroid::GetHistorySize() const {
  return Java_MotionEvent_getHistorySize(AttachCurrentThread(), event_.obj());
}

base::TimeTicks MotionEventAndroid::GetHistoricalEventTime(
    size_t historical_index) const {
  return FromAndroidTime(Java_MotionEvent_getHistoricalEventTime(
      AttachCurrentThread(), event_.obj(), historical_index));
}

float MotionEventAndroid::GetHistoricalTouchMajor(size_t pointer_index,
                                                  size_t historical_index)
    const {
  return Java_MotionEvent_getHistoricalTouchMajorF_I_I(
      AttachCurrentThread(), event_.obj(), pointer_index, historical_index);
}

float MotionEventAndroid::GetHistoricalX(size_t pointer_index,
                                         size_t historical_index) const {
  return Java_MotionEvent_getHistoricalXF_I_I(
      AttachCurrentThread(), event_.obj(), pointer_index, historical_index);
}

float MotionEventAndroid::GetHistoricalY(size_t pointer_index,
                                         size_t historical_index) const {
  return Java_MotionEvent_getHistoricalYF_I_I(
      AttachCurrentThread(), event_.obj(), pointer_index, historical_index);
}

// static
bool MotionEventAndroid::RegisterMotionEventAndroid(JNIEnv* env) {
  return JNI_MotionEvent::RegisterNativesImpl(env);
}

// static
scoped_ptr<MotionEventAndroid> MotionEventAndroid::Obtain(
    const MotionEventAndroid& event) {
  return make_scoped_ptr(new MotionEventAndroid(
      Java_MotionEvent_obtainAVME_AVME(AttachCurrentThread(),
                                       event.event_.obj()),
      true));
}

// static
scoped_ptr<MotionEventAndroid> MotionEventAndroid::Obtain(
    base::TimeTicks down_time,
    base::TimeTicks event__time,
    Action action,
    float x,
    float y) {
  return make_scoped_ptr(new MotionEventAndroid(
      Java_MotionEvent_obtainAVME_J_J_I_F_F_I(AttachCurrentThread(),
                                              ToAndroidTime(down_time),
                                              ToAndroidTime(down_time),
                                              action,
                                              x,
                                              y,
                                              0),
      true));
}

}  // namespace content
