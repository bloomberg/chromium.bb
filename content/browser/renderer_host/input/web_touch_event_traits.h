// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_WEB_TOUCH_EVENT_TRAITS_H_
#define CONTENT_COMMON_INPUT_WEB_TOUCH_EVENT_TRAITS_H_

#include "base/basictypes.h"
#include "content/common/input/scoped_web_input_event.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

// Utility class for performing operations on and with WebTouchEvents.
class CONTENT_EXPORT WebTouchEventTraits {
 public:
  static bool IsTouchSequenceStart(const blink::WebTouchEvent& event);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_WEB_TOUCH_EVENT_TRAITS_H_
