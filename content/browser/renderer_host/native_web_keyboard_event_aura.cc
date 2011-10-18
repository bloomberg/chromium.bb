// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_web_keyboard_event.h"

#include "content/browser/renderer_host/web_input_event_aura.h"

using WebKit::WebKeyboardEvent;

NativeWebKeyboardEvent::NativeWebKeyboardEvent()
    : os_event(NULL),
      skip_in_browser(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(gfx::NativeEvent native_event)
    : WebKeyboardEvent(content::MakeWebKeyboardEvent(
          reinterpret_cast<aura::KeyEvent*>(native_event))),
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
}
