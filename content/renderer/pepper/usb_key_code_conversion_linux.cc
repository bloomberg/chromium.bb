// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/usb_key_code_conversion.h"

#include "base/basictypes.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/keycodes/dom4/keycode_converter.h"

using blink::WebKeyboardEvent;

namespace content {

uint32_t UsbKeyCodeForKeyboardEvent(const WebKeyboardEvent& key_event) {
  // TODO(garykac): This code assumes that on Linux we're receiving events via
  // the XKB driver.  We should detect between "XKB", "kbd" and "evdev" at
  // run-time and re-map accordingly, but that's not possible here, inside the
  // sandbox.
  return ui::KeycodeConverter::NativeKeycodeToUsbKeycode(
      key_event.nativeKeyCode);
}

const char* CodeForKeyboardEvent(const WebKeyboardEvent& key_event) {
  return ui::KeycodeConverter::NativeKeycodeToCode(key_event.nativeKeyCode);
}

}  // namespace content
