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

int ToAndroidAction(MotionEventAndroid::Action action) {
  switch (action) {
    case MotionEventAndroid::ACTION_DOWN:
      return ACTION_DOWN;
    case MotionEventAndroid::ACTION_UP:
      return ACTION_UP;
    case MotionEventAndroid::ACTION_MOVE:
      return ACTION_MOVE;
    case MotionEventAndroid::ACTION_CANCEL:
      return ACTION_CANCEL;
    case MotionEventAndroid::ACTION_POINTER_DOWN:
      return ACTION_POINTER_DOWN;
    case MotionEventAndroid::ACTION_POINTER_UP:
      return ACTION_POINTER_UP;
  };
  NOTREACHED() << "Invalid Android MotionEvent type for gesture detection: "
               << action;
  return ACTION_CANCEL;
}

MotionEventAndroid::Action FromAndroidAction(int android_action) {
  switch (android_action) {
    case ACTION_DOWN:
      return MotionEventAndroid::ACTION_DOWN;
    case ACTION_UP:
      return MotionEventAndroid::ACTION_UP;
    case ACTION_MOVE:
      return MotionEventAndroid::ACTION_MOVE;
    case ACTION_CANCEL:
      return MotionEventAndroid::ACTION_CANCEL;
    case ACTION_POINTER_DOWN:
      return MotionEventAndroid::ACTION_POINTER_DOWN;
    case ACTION_POINTER_UP:
      return MotionEventAndroid::ACTION_POINTER_UP;
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

MotionEventAndroid::MotionEventAndroid(JNIEnv* env, jobject event, bool recycle)
    : cached_time_(FromAndroidTime(Java_MotionEvent_getEventTime(env, event))),
      cached_action_(FromAndroidAction(
          Java_MotionEvent_getActionMasked(env, event))),
      cached_pointer_count_(Java_MotionEvent_getPointerCount(env, event)),
      cached_history_size_(Java_MotionEvent_getHistorySize(env, event)),
      cached_action_index_(Java_MotionEvent_getActionIndex(env, event)),
      cached_x_(Java_MotionEvent_getXF(env, event)),
      cached_y_(Java_MotionEvent_getYF(env, event)),
      should_recycle_(recycle) {
  event_.Reset(env, event);
  DCHECK(event_.obj());
}

MotionEventAndroid::MotionEventAndroid(const MotionEventAndroid& other,
                                       bool clone)
    : cached_time_(other.cached_time_),
      cached_action_(other.cached_action_),
      cached_pointer_count_(other.cached_pointer_count_),
      cached_history_size_(other.cached_history_size_),
      cached_action_index_(other.cached_action_index_),
      cached_x_(other.cached_x_),
      cached_y_(other.cached_y_),
      should_recycle_(clone) {
  // An event with a pending recycle should never be copied (only cloned).
  DCHECK(clone || !other.should_recycle_);
  if (clone)
    event_.Reset(Obtain(other));
  else
    event_.Reset(other.event_);
}

MotionEventAndroid::~MotionEventAndroid() {
  if (should_recycle_)
    Java_MotionEvent_recycle(AttachCurrentThread(), event_.obj());
}

MotionEventAndroid::Action MotionEventAndroid::GetAction() const {
  return cached_action_;
}

int MotionEventAndroid::GetActionIndex() const {
  return cached_action_index_;
}

size_t MotionEventAndroid::GetPointerCount() const {
  return cached_pointer_count_;
}

int MotionEventAndroid::GetPointerId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  return Java_MotionEvent_getPointerId(
      AttachCurrentThread(), event_.obj(), pointer_index);
}

float MotionEventAndroid::GetX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index == 0)
    return cached_x_;
  return Java_MotionEvent_getXF_I(
      AttachCurrentThread(), event_.obj(), pointer_index);
}

float MotionEventAndroid::GetY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  if (pointer_index == 0)
    return cached_y_;
  return Java_MotionEvent_getYF_I(
      AttachCurrentThread(), event_.obj(), pointer_index);
}

float MotionEventAndroid::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, cached_pointer_count_);
  return Java_MotionEvent_getTouchMajorF_I(
      AttachCurrentThread(), event_.obj(), pointer_index);
}

base::TimeTicks MotionEventAndroid::GetEventTime() const {
  return cached_time_;
}

size_t MotionEventAndroid::GetHistorySize() const {
  return cached_history_size_;
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

scoped_ptr<ui::MotionEvent> MotionEventAndroid::Clone() const {
  return scoped_ptr<MotionEvent>(new MotionEventAndroid(*this, true));
}

scoped_ptr<ui::MotionEvent> MotionEventAndroid::Cancel() const {
  return scoped_ptr<MotionEvent>(new MotionEventAndroid(
      AttachCurrentThread(),
      Obtain(GetDownTime(),
             GetEventTime(),
             MotionEventAndroid::ACTION_CANCEL,
             GetX(0),
             GetY(0)).obj(),
      true));
}

float MotionEventAndroid::GetPressure(size_t pointer_index) const {
  return Java_MotionEvent_getPressureF_I(
      AttachCurrentThread(), event_.obj(), pointer_index);
}

float MotionEventAndroid::GetTouchMinor(size_t pointer_index) const {
  return Java_MotionEvent_getTouchMinorF_I(
      AttachCurrentThread(), event_.obj(), pointer_index);
}

float MotionEventAndroid::GetOrientation() const {
  return Java_MotionEvent_getOrientationF(AttachCurrentThread(), event_.obj());
}

base::TimeTicks MotionEventAndroid::GetDownTime() const {
  return FromAndroidTime(
      Java_MotionEvent_getDownTime(AttachCurrentThread(), event_.obj()));
}

// static
bool MotionEventAndroid::RegisterMotionEventAndroid(JNIEnv* env) {
  return JNI_MotionEvent::RegisterNativesImpl(env);
}

// static
base::android::ScopedJavaLocalRef<jobject> MotionEventAndroid::Obtain(
    const MotionEventAndroid& event) {
  return Java_MotionEvent_obtainAVME_AVME(AttachCurrentThread(),
                                          event.event_.obj());
}

// static
base::android::ScopedJavaLocalRef<jobject> MotionEventAndroid::Obtain(
    base::TimeTicks down_time,
    base::TimeTicks event_time,
    Action action,
    float x,
    float y) {
  return Java_MotionEvent_obtainAVME_J_J_I_F_F_I(AttachCurrentThread(),
                                                 ToAndroidTime(down_time),
                                                 ToAndroidTime(event_time),
                                                 ToAndroidAction(action),
                                                 x,
                                                 y,
                                                 0);
}

}  // namespace content
