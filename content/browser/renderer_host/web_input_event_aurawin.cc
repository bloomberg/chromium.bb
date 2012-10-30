// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_input_event_aura.h"

#include "base/event_types.h"
#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/win/WebInputEventFactory.h"

namespace content {

// On Windows, we can just use the builtin WebKit factory methods to fully
// construct our pre-translated events.

WebKit::WebMouseEvent MakeUntranslatedWebMouseEventFromNativeEvent(
    base::NativeEvent native_event) {
  return WebKit::WebInputEventFactory::mouseEvent(native_event.hwnd,
                                                  native_event.message,
                                                  native_event.wParam,
                                                  native_event.lParam);
}

WebKit::WebMouseWheelEvent MakeUntranslatedWebMouseWheelEventFromNativeEvent(
    base::NativeEvent native_event) {
  return WebKit::WebInputEventFactory::mouseWheelEvent(native_event.hwnd,
                                                       native_event.message,
                                                       native_event.wParam,
                                                       native_event.lParam);
}

WebKit::WebKeyboardEvent MakeWebKeyboardEventFromNativeEvent(
    base::NativeEvent native_event) {
  return WebKit::WebInputEventFactory::keyboardEvent(native_event.hwnd,
                                                     native_event.message,
                                                     native_event.wParam,
                                                     native_event.lParam);
}

WebKit::WebGestureEvent MakeWebGestureEventFromNativeEvent(
    base::NativeEvent native_event) {
  // TODO: Create gestures from native event.
  NOTIMPLEMENTED();
  return  WebKit::WebGestureEvent();
}

}  // namespace content
