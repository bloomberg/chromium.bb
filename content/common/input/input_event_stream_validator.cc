// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_event_stream_validator.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

using blink::WebInputEvent;
using blink::WebGestureEvent;

InputEventStreamValidator::InputEventStreamValidator() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  enabled_ = command_line->HasSwitch(switches::kValidateInputEventStream);
}
InputEventStreamValidator::~InputEventStreamValidator() {}

void InputEventStreamValidator::OnEvent(const WebInputEvent& event) {
  if (!enabled_)
    return;
  const char* error_message = NULL;
  if (WebInputEvent::isGestureEventType(event.type)) {
    const WebGestureEvent& gesture_event =
        static_cast<const WebGestureEvent&>(event);
    if (gesture_event.sourceDevice == blink::WebGestureEvent::Touchscreen) {
      if (!gesture_validator_.Validate(gesture_event, &error_message))
        NOTREACHED() << error_message;
    }
  }
}

}  // namespace content
