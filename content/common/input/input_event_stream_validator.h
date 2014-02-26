// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_EVENT_STREAM_VALIDATOR
#define CONTENT_COMMON_INPUT_EVENT_STREAM_VALIDATOR

#include "content/common/input/gesture_event_stream_validator.h"

namespace blink {
class WebInputEvent;
}

namespace content {

// DCHECKs that the stream of WebInputEvents passed to OnEvent is
// valid. Currently only validates WebGestureEvents.
class InputEventStreamValidator {
 public:
  InputEventStreamValidator();
  ~InputEventStreamValidator();
  void OnEvent(const blink::WebInputEvent&);

 private:
  GestureEventStreamValidator gesture_validator_;
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(InputEventStreamValidator);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_EVENT_STREAM_VALIDATOR
