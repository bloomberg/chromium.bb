// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/android_ui_gesture_target.h"

#include "jni/MotionEventSynthesizer_jni.h"

namespace vr_shell {

AndroidUiGestureTarget::AndroidUiGestureTarget(jobject event_synthesizer,
                                               float scroll_ratio)
    : event_synthesizer_(event_synthesizer), scroll_ratio_(scroll_ratio) {
  DCHECK(event_synthesizer_);
}

AndroidUiGestureTarget::~AndroidUiGestureTarget() = default;

void AndroidUiGestureTarget::DispatchWebInputEvent(
    std::unique_ptr<blink::WebInputEvent> event) {
  JNIEnv* env = base::android::AttachCurrentThread();
  blink::WebMouseEvent* mouse;
  blink::WebGestureEvent* gesture;
  if (blink::WebInputEvent::IsMouseEventType(event->GetType())) {
    mouse = static_cast<blink::WebMouseEvent*>(event.get());
  } else {
    gesture = static_cast<blink::WebGestureEvent*>(event.get());
  }

  switch (event->GetType()) {
    case blink::WebGestureEvent::kGestureScrollBegin:
      DCHECK(gesture->data.scroll_begin.delta_hint_units ==
             blink::WebGestureEvent::ScrollUnits::kPrecisePixels);
      scroll_x_ = (scroll_ratio_ * gesture->data.scroll_begin.delta_x_hint);
      scroll_y_ = (scroll_ratio_ * gesture->data.scroll_begin.delta_y_hint);
      SetPointer(env, 0, 0);
      Inject(env, Action::Start, gesture->TimeStampSeconds());
      SetPointer(env, scroll_x_, scroll_y_);
      // Send a move immediately so that we can't accidentally trigger a click.
      Inject(env, Action::Move, gesture->TimeStampSeconds());
      break;
    case blink::WebGestureEvent::kGestureScrollEnd:
      Inject(env, Action::End, gesture->TimeStampSeconds());
      break;
    case blink::WebGestureEvent::kGestureScrollUpdate:
      scroll_x_ += (scroll_ratio_ * gesture->data.scroll_update.delta_x);
      scroll_y_ += (scroll_ratio_ * gesture->data.scroll_update.delta_y);
      SetPointer(env, scroll_x_, scroll_y_);
      Inject(env, Action::Move, gesture->TimeStampSeconds());
      break;
    case blink::WebGestureEvent::kGestureTapDown:
      SetPointer(env, gesture->x, gesture->y);
      Inject(env, Action::Start, gesture->TimeStampSeconds());
      Inject(env, Action::End, gesture->TimeStampSeconds());
      break;
    case blink::WebGestureEvent::kGestureFlingCancel:
      Inject(env, Action::Cancel, gesture->TimeStampSeconds());
      break;
    case blink::WebGestureEvent::kGestureFlingStart:
      // Flings are automatically generated for android UI. Ignore this input.
      break;
    case blink::WebMouseEvent::kMouseEnter:
      SetPointer(env, mouse->PositionInWidget().x, mouse->PositionInWidget().y);
      Inject(env, Action::HoverEnter, gesture->TimeStampSeconds());
      break;
    case blink::WebMouseEvent::kMouseMove:
      SetPointer(env, mouse->PositionInWidget().x, mouse->PositionInWidget().y);
      Inject(env, Action::HoverMove, gesture->TimeStampSeconds());
      break;
    case blink::WebMouseEvent::kMouseLeave:
      SetPointer(env, mouse->PositionInWidget().x, mouse->PositionInWidget().y);
      Inject(env, Action::HoverExit, gesture->TimeStampSeconds());
      break;
    case blink::WebMouseEvent::kMouseDown:
      // Mouse down events are translated into touch events on Android anyways,
      // so we can just send touch events.
      // We intentionally don't support long press or drags/swipes with mouse
      // input as this could trigger long press and open 2D popups.
      SetPointer(env, mouse->PositionInWidget().x, mouse->PositionInWidget().y);
      Inject(env, Action::Start, gesture->TimeStampSeconds());
      Inject(env, Action::End, gesture->TimeStampSeconds());
      break;
    case blink::WebMouseEvent::kMouseUp:
      // No need to do anything for mouseUp as mouseDown already handled up.
      break;
    default:
      NOTREACHED() << "Unsupported event type sent to Android UI.";
      break;
  }
}

void AndroidUiGestureTarget::SetPointer(JNIEnv* env, int x, int y) {
  content::Java_MotionEventSynthesizer_setPointer(env, event_synthesizer_, 0, x,
                                                  y, 0);
}

void AndroidUiGestureTarget::SetScrollDeltas(JNIEnv* env,
                                             int x,
                                             int y,
                                             int dx,
                                             int dy) {
  content::Java_MotionEventSynthesizer_setScrollDeltas(env, event_synthesizer_,
                                                       x, y, dx, dy);
}

void AndroidUiGestureTarget::Inject(JNIEnv* env,
                                    Action action,
                                    double time_in_seconds) {
  content::Java_MotionEventSynthesizer_inject(
      env, event_synthesizer_, static_cast<int>(action), 1,
      static_cast<int64_t>(time_in_seconds * 1000.0));
}

}  // namespace vr_shell
