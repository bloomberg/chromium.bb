// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"

#include "base/logging.h"
#include "content/common/input/input_event.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/latency_info.h"

namespace content {
namespace {

void DispatchEventToPlatform(SyntheticGestureTarget* target,
                             const blink::WebInputEvent& event) {
  target->DispatchInputEventToPlatform(
      InputEvent(event, ui::LatencyInfo(), false));
}

}  // namespace

SyntheticTapGesture::SyntheticTapGesture(
    const SyntheticTapGestureParams& params)
    : params_(params),
      gesture_source_type_(SyntheticGestureParams::DEFAULT_INPUT),
      state_(SETUP) {
  DCHECK_GE(params_.duration_ms, 0);
}

SyntheticTapGesture::~SyntheticTapGesture() {}

SyntheticGesture::Result SyntheticTapGesture::ForwardInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {
  if (state_ == SETUP) {
    gesture_source_type_ = params_.gesture_source_type;
    if (gesture_source_type_ == SyntheticGestureParams::DEFAULT_INPUT)
      gesture_source_type_ = target->GetDefaultSyntheticGestureSourceType();

    if (!target->SupportsSyntheticGestureSourceType(gesture_source_type_))
      return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_SUPPORTED_BY_PLATFORM;

    state_ = PRESS;
  }

  DCHECK_NE(gesture_source_type_, SyntheticGestureParams::DEFAULT_INPUT);
  if (gesture_source_type_ == SyntheticGestureParams::TOUCH_INPUT ||
      gesture_source_type_ == SyntheticGestureParams::MOUSE_INPUT)
    ForwardTouchOrMouseInputEvents(interval, target);
  else
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;

  return (state_ == DONE) ? SyntheticGesture::GESTURE_FINISHED
                          : SyntheticGesture::GESTURE_RUNNING;
}

void SyntheticTapGesture::ForwardTouchOrMouseInputEvents(
    const base::TimeDelta& interval, SyntheticGestureTarget* target) {
  switch (state_) {
    case PRESS:
      Press(target);
      // Release immediately if duration is 0.
      if (params_.duration_ms == 0) {
        Release(target);
        state_ = DONE;
      } else {
        state_ = WAITING_TO_RELEASE;
      }
      break;
    case WAITING_TO_RELEASE:
      total_waiting_time_ += interval;
      if (total_waiting_time_ >=
          base::TimeDelta::FromMilliseconds(params_.duration_ms)) {
        Release(target);
        state_ = DONE;
      }
      break;
    case SETUP:
      NOTREACHED() << "State SETUP invalid for synthetic tap gesture.";
    case DONE:
      NOTREACHED() << "State DONE invalid for synthetic tap gesture.";
  }
}

void SyntheticTapGesture::Press(SyntheticGestureTarget* target) {
  if (gesture_source_type_ == SyntheticGestureParams::TOUCH_INPUT) {
    touch_event_.PressPoint(params_.position.x(), params_.position.y());
    DispatchEventToPlatform(target, touch_event_);
  } else if (gesture_source_type_ == SyntheticGestureParams::MOUSE_INPUT) {
    blink::WebMouseEvent mouse_event =
        SyntheticWebMouseEventBuilder::Build(blink::WebInputEvent::MouseDown,
                                             params_.position.x(),
                                             params_.position.y(),
                                             0);
    mouse_event.clickCount = 1;
    DispatchEventToPlatform(target, mouse_event);
  } else {
    NOTREACHED() << "Invalid gesture source type for synthetic tap gesture.";
  }
}

void SyntheticTapGesture::Release(SyntheticGestureTarget* target) {
  if (gesture_source_type_ == SyntheticGestureParams::TOUCH_INPUT) {
    touch_event_.ReleasePoint(0);
    DispatchEventToPlatform(target, touch_event_);
  } else if (gesture_source_type_ == SyntheticGestureParams::MOUSE_INPUT) {
    blink::WebMouseEvent mouse_event =
        SyntheticWebMouseEventBuilder::Build(blink::WebInputEvent::MouseUp,
                                             params_.position.x(),
                                             params_.position.y(),
                                             0);
    mouse_event.clickCount = 1;
    DispatchEventToPlatform(target, mouse_event);
  } else {
    NOTREACHED() << "Invalid gesture source type for synthetic tap gesture.";
  }
}

}  // namespace content
