// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/android_ui_gesture_target.h"

#include "jni/AndroidUiGestureTarget_jni.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using content::MotionEventAction;

namespace vr_shell {

AndroidUiGestureTarget::AndroidUiGestureTarget(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               float scale_factor,
                                               float scroll_ratio)
    : scale_factor_(scale_factor),
      scroll_ratio_(scroll_ratio),
      java_ref_(env, obj) {}

AndroidUiGestureTarget::~AndroidUiGestureTarget() = default;

void AndroidUiGestureTarget::DispatchWebInputEvent(
    std::unique_ptr<blink::WebInputEvent> event) {
  blink::WebMouseEvent* mouse;
  blink::WebGestureEvent* gesture;
  if (blink::WebInputEvent::IsMouseEventType(event->GetType())) {
    mouse = static_cast<blink::WebMouseEvent*>(event.get());
  } else {
    gesture = static_cast<blink::WebGestureEvent*>(event.get());
  }

  int64_t event_time_ms = event->TimeStampSeconds() * 1000;
  switch (event->GetType()) {
    case blink::WebGestureEvent::kGestureScrollBegin:
      DCHECK(gesture->data.scroll_begin.delta_hint_units ==
             blink::WebGestureEvent::ScrollUnits::kPrecisePixels);
      scroll_x_ = (scroll_ratio_ * gesture->data.scroll_begin.delta_x_hint);
      scroll_y_ = (scroll_ratio_ * gesture->data.scroll_begin.delta_y_hint);
      SetPointer(0, 0);
      Inject(content::MOTION_EVENT_ACTION_START, event_time_ms);
      SetPointer(scroll_x_, scroll_y_);
      // Send a move immediately so that we can't accidentally trigger a click.
      Inject(content::MOTION_EVENT_ACTION_MOVE, event_time_ms);
      break;
    case blink::WebGestureEvent::kGestureScrollEnd:
      Inject(content::MOTION_EVENT_ACTION_END, event_time_ms);
      break;
    case blink::WebGestureEvent::kGestureScrollUpdate:
      scroll_x_ += (scroll_ratio_ * gesture->data.scroll_update.delta_x);
      scroll_y_ += (scroll_ratio_ * gesture->data.scroll_update.delta_y);
      SetPointer(scroll_x_, scroll_y_);
      Inject(content::MOTION_EVENT_ACTION_MOVE, event_time_ms);
      break;
    case blink::WebGestureEvent::kGestureTapDown:
      SetPointer(gesture->x, gesture->y);
      Inject(content::MOTION_EVENT_ACTION_START, event_time_ms);
      Inject(content::MOTION_EVENT_ACTION_END, event_time_ms);
      break;
    case blink::WebGestureEvent::kGestureFlingCancel:
      Inject(content::MOTION_EVENT_ACTION_CANCEL, event_time_ms);
      break;
    case blink::WebGestureEvent::kGestureFlingStart:
      // Flings are automatically generated for android UI. Ignore this input.
      break;
    case blink::WebMouseEvent::kMouseEnter:
      SetPointer(mouse->PositionInWidget().x, mouse->PositionInWidget().y);
      Inject(content::MOTION_EVENT_ACTION_HOVER_ENTER, event_time_ms);
      break;
    case blink::WebMouseEvent::kMouseMove:
      SetPointer(mouse->PositionInWidget().x, mouse->PositionInWidget().y);
      Inject(content::MOTION_EVENT_ACTION_HOVER_MOVE, event_time_ms);
      break;
    case blink::WebMouseEvent::kMouseLeave:
      SetPointer(mouse->PositionInWidget().x, mouse->PositionInWidget().y);
      Inject(content::MOTION_EVENT_ACTION_HOVER_EXIT, event_time_ms);
      break;
    case blink::WebMouseEvent::kMouseDown:
      // Mouse down events are translated into touch events on Android anyways,
      // so we can just send touch events.
      // We intentionally don't support long press or drags/swipes with mouse
      // input as this could trigger long press and open 2D popups.
      SetPointer(mouse->PositionInWidget().x, mouse->PositionInWidget().y);
      Inject(content::MOTION_EVENT_ACTION_START, event_time_ms);
      Inject(content::MOTION_EVENT_ACTION_END, event_time_ms);
      break;
    case blink::WebMouseEvent::kMouseUp:
      // No need to do anything for mouseUp as mouseDown already handled up.
      break;
    default:
      NOTREACHED() << "Unsupported event type sent to Android UI.";
      break;
  }
}

void AndroidUiGestureTarget::Inject(MotionEventAction action, int64_t time_ms) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AndroidUiGestureTarget_inject(env, obj, action, time_ms);
}

void AndroidUiGestureTarget::SetPointer(int x, int y) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AndroidUiGestureTarget_setPointer(env, obj, x * scale_factor_,
                                         y * scale_factor_);
}

// static
AndroidUiGestureTarget* AndroidUiGestureTarget::FromJavaObject(
    const JavaRef<jobject>& obj) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return reinterpret_cast<AndroidUiGestureTarget*>(
      Java_AndroidUiGestureTarget_getNativeObject(env, obj));
}

static jlong JNI_AndroidUiGestureTarget_Init(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             jfloat scale_factor,
                                             jfloat scroll_ratio) {
  return reinterpret_cast<intptr_t>(
      new AndroidUiGestureTarget(env, obj, scale_factor, scroll_ratio));
}

}  // namespace vr_shell
