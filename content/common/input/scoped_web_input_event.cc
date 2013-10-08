// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/scoped_web_input_event.h"

#include "base/logging.h"
#include "content/common/input/web_input_event_traits.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebTouchEvent;

namespace content {

WebInputEventDeleter::WebInputEventDeleter() {}

void WebInputEventDeleter::operator()(WebInputEvent* web_event) const {
  WebInputEventTraits::Delete(web_event);
}

}  // namespace content
