// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_web_keyboard_event.h"

#include "content/browser/renderer_host/input/web_input_event_builders_win.h"

using WebKit::WebKeyboardEvent;

namespace content {

NativeWebKeyboardEvent::NativeWebKeyboardEvent()
    : skip_in_browser(false) {
  memset(&os_event, 0, sizeof(os_event));
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(gfx::NativeEvent native_event)
    : WebKeyboardEvent(
          WebKeyboardEventBuilder::Build(native_event.hwnd,
                                         native_event.message,
                                         native_event.wParam,
                                         native_event.lParam)),
      os_event(native_event),
      skip_in_browser(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& other)
    : WebKeyboardEvent(other),
      os_event(other.os_event),
      skip_in_browser(other.skip_in_browser) {
}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  WebKeyboardEvent::operator=(other);

  os_event = other.os_event;
  skip_in_browser = other.skip_in_browser;

  return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {
  // Noop under windows
}

}  // namespace content
