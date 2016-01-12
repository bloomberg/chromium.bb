// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_input_event_aura.h"

#include "base/event_types.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/web_input_event_builders_win.h"

namespace content {

// On Windows, we can just use the builtin WebKit factory methods to fully
// construct our pre-translated events.

blink::WebMouseEvent MakeUntranslatedWebMouseEventFromNativeEvent(
    const base::NativeEvent& native_event,
    const base::TimeDelta& time_stamp) {
  return WebMouseEventBuilder::Build(native_event.hwnd, native_event.message,
                                     native_event.wParam, native_event.lParam,
                                     time_stamp.InSecondsF());
}

blink::WebMouseWheelEvent MakeUntranslatedWebMouseWheelEventFromNativeEvent(
    const base::NativeEvent& native_event,
    const base::TimeDelta& time_stamp) {
  return WebMouseWheelEventBuilder::Build(
      native_event.hwnd, native_event.message, native_event.wParam,
      native_event.lParam, time_stamp.InSecondsF());
}

blink::WebKeyboardEvent MakeWebKeyboardEventFromNativeEvent(
    const base::NativeEvent& native_event,
    const base::TimeDelta& time_stamp) {
  return WebKeyboardEventBuilder::Build(
      native_event.hwnd, native_event.message, native_event.wParam,
      native_event.lParam, time_stamp.InSecondsF());
}

blink::WebGestureEvent MakeWebGestureEventFromNativeEvent(
    const base::NativeEvent& native_event,
    const base::TimeDelta& time_stamp) {
  // TODO: Create gestures from native event.
  NOTIMPLEMENTED();
  return  blink::WebGestureEvent();
}

}  // namespace content
