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
  if (blink::WebInputEvent::isMouseEventType(event->type())) {
    mouse = static_cast<blink::WebMouseEvent*>(event.get());
  } else {
    gesture = static_cast<blink::WebGestureEvent*>(event.get());
  }

  switch (event->type()) {
    case blink::WebGestureEvent::GestureScrollBegin:
      DCHECK(gesture->data.scrollBegin.deltaHintUnits ==
             blink::WebGestureEvent::ScrollUnits::PrecisePixels);
      scroll_x_ = (scroll_ratio_ * gesture->data.scrollBegin.deltaXHint);
      scroll_y_ = (scroll_ratio_ * gesture->data.scrollBegin.deltaYHint);
      SetPointer(env, 0, 0);
      Inject(env, Action::Start, gesture->timeStampSeconds());
      SetPointer(env, scroll_x_, scroll_y_);
      // Send a move immediately so that we can't accidentally trigger a click.
      Inject(env, Action::Move, gesture->timeStampSeconds());
      break;
    case blink::WebGestureEvent::GestureScrollEnd:
      Inject(env, Action::End, gesture->timeStampSeconds());
      break;
    case blink::WebGestureEvent::GestureScrollUpdate:
      scroll_x_ += (scroll_ratio_ * gesture->data.scrollUpdate.deltaX);
      scroll_y_ += (scroll_ratio_ * gesture->data.scrollUpdate.deltaY);
      SetPointer(env, scroll_x_, scroll_y_);
      Inject(env, Action::Move, gesture->timeStampSeconds());
      break;
    case blink::WebGestureEvent::GestureTapDown:
      SetPointer(env, gesture->x, gesture->y);
      Inject(env, Action::Start, gesture->timeStampSeconds());
      Inject(env, Action::End, gesture->timeStampSeconds());
      break;
    case blink::WebGestureEvent::GestureFlingCancel:
      Inject(env, Action::Cancel, gesture->timeStampSeconds());
      break;
    case blink::WebGestureEvent::GestureFlingStart:
      // Flings are automatically generated for android UI. Ignore this input.
      break;
    case blink::WebMouseEvent::MouseEnter:
      SetPointer(env, mouse->x, mouse->y);
      Inject(env, Action::HoverEnter, gesture->timeStampSeconds());
      break;
    case blink::WebMouseEvent::MouseMove:
      SetPointer(env, mouse->x, mouse->y);
      Inject(env, Action::HoverMove, gesture->timeStampSeconds());
      break;
    case blink::WebMouseEvent::MouseLeave:
      SetPointer(env, mouse->x, mouse->y);
      Inject(env, Action::HoverExit, gesture->timeStampSeconds());
      break;
    default:
      NOTREACHED();
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
