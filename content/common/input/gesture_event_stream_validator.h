// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_GESTURE_EVENT_STREAM_VALIDATOR
#define CONTENT_COMMON_INPUT_GESTURE_EVENT_STREAM_VALIDATOR

#include "base/basictypes.h"

namespace blink {
class WebGestureEvent;
}

namespace content {

// In debug mode, logs an error and exits if the stream of WebGestureEvents
// passed to Validate is invalid.
class GestureEventStreamValidator {
 public:
  GestureEventStreamValidator();
  ~GestureEventStreamValidator();
  // Returns true iff |event| is valid for the current stream.
  bool Validate(const blink::WebGestureEvent& event,
                const char** error_message);

 private:
  bool scrolling_;
  bool pinching_;
  bool waiting_for_tap_end_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventStreamValidator);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_GESTURE_EVENT_STREAM_VALIDATOR
