// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/gesture_event_stream_validator.h"

#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

using blink::WebInputEvent;

GestureEventStreamValidator::GestureEventStreamValidator()
    : scrolling_(false), pinching_(false), waiting_for_tap_end_(false) {}

GestureEventStreamValidator::~GestureEventStreamValidator() {}

bool GestureEventStreamValidator::Validate(const blink::WebGestureEvent& event,
                                           const char** error_message) {
  *error_message = NULL;
  switch (event.type) {
    case WebInputEvent::GestureScrollBegin:
      if (scrolling_ || pinching_)
        *error_message = "Scroll begin during scroll";
      scrolling_ = true;
      break;
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureScrollUpdateWithoutPropagation:
      if (!scrolling_)
        *error_message = "Scroll update outside of scroll";
      break;
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureFlingStart:
      if (!scrolling_)
        *error_message = "Scroll end outside of scroll";
      if (pinching_)
        *error_message = "Ending scroll while pinching";
      scrolling_ = false;
      break;
    case WebInputEvent::GesturePinchBegin:
      if (!scrolling_)
        *error_message = "Pinch begin outside of scroll";
      if (pinching_)
        *error_message = "Pinch begin during pinch";
      pinching_ = true;
      break;
    case WebInputEvent::GesturePinchUpdate:
      if (!pinching_ || !scrolling_)
        *error_message = "Pinch update outside of pinch";
      break;
    case WebInputEvent::GesturePinchEnd:
      if (!pinching_ || !scrolling_)
        *error_message = "Pinch end outside of pinch";
      pinching_ = false;
      break;
    case WebInputEvent::GestureTapDown:
      if (waiting_for_tap_end_)
        *error_message = "Missing tap end event";
      waiting_for_tap_end_ = true;
      break;
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureDoubleTap:
      if (!waiting_for_tap_end_)
        *error_message = "Missing GestureTapDown event";
      waiting_for_tap_end_ = false;
      break;
    default:
      break;
  }
  if (*error_message == NULL)
    return true;
  return false;
}

}  // namespace content
