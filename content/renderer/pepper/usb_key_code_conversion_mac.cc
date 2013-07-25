// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/usb_key_code_conversion.h"

#include "base/basictypes.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using WebKit::WebKeyboardEvent;

namespace content {

namespace {

#define USB_KEYMAP(usb, xkb, win, mac) {usb, mac}
#include "ui/base/keycodes/usb_keycode_map.h"
#undef USB_KEYMAP

}  // anonymous namespace

uint32_t UsbKeyCodeForKeyboardEvent(const WebKeyboardEvent& key_event) {
  return NativeKeycodeToUsbKeycode(key_event.nativeKeyCode);
}

}  // namespace content
