// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/web_touch_event_traits.h"

#include "base/logging.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;

namespace content {

namespace {

bool AllTouchPointsHaveState(const WebTouchEvent& event,
                             blink::WebTouchPoint::State state) {
  if(!event.touchesLength)
    return false;
  for (size_t i = 0; i < event.touchesLength; ++i) {
    if (event.touches[i].state != state)
      return false;
  }
  return true;
}

}  // namespace

bool WebTouchEventTraits::IsTouchSequenceStart(const WebTouchEvent& event) {
  DCHECK(event.touchesLength);
  if (event.type != WebInputEvent::TouchStart)
    return false;
  return AllTouchPointsHaveState(event, blink::WebTouchPoint::StatePressed);
}

}  // namespace content
