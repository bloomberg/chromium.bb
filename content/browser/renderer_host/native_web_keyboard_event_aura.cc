// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_web_keyboard_event.h"

#include "base/logging.h"
#include "content/browser/renderer_host/web_input_event_aura.h"
#include "ui/aura/event.h"

namespace {

// We need to copy |os_event| in NativeWebKeyboardEvent because it is
// queued in RenderWidgetHost and may be passed and used
// RenderViewHostDelegate::HandledKeybardEvent after the original aura
// event is destroyed.
aura::Event* CopyEvent(aura::Event* event) {
  return event ? static_cast<aura::KeyEvent*>(event)->Copy() : NULL;
}

int EventFlagsToWebInputEventModifiers(int flags) {
  return
      (flags & ui::EF_SHIFT_DOWN ? WebKit::WebInputEvent::ShiftKey : 0) |
      (flags & ui::EF_CONTROL_DOWN ? WebKit::WebInputEvent::ControlKey : 0) |
      (flags & ui::EF_CAPS_LOCK_DOWN ? WebKit::WebInputEvent::CapsLockOn : 0) |
      (flags & ui::EF_ALT_DOWN ? WebKit::WebInputEvent::AltKey : 0);
}

}  // namespace

using WebKit::WebKeyboardEvent;

namespace content {

NativeWebKeyboardEvent::NativeWebKeyboardEvent()
    : os_event(NULL),
      skip_in_browser(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(gfx::NativeEvent native_event)
    : WebKeyboardEvent(content::MakeWebKeyboardEvent(
          static_cast<aura::KeyEvent*>(native_event))),
      os_event(CopyEvent(native_event)),
      skip_in_browser(false) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& other)
    : WebKeyboardEvent(other),
      os_event(CopyEvent(other.os_event)),
      skip_in_browser(other.skip_in_browser) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    ui::EventType key_event_type,
    bool is_char,
    wchar_t character,
    int state,
    double time_stamp_seconds)
    : os_event(NULL),
      skip_in_browser(true /* already handled by the input method */) {
  switch (key_event_type) {
    case ui::ET_KEY_PRESSED:
      type = is_char ? WebKit::WebInputEvent::Char :
          WebKit::WebInputEvent::RawKeyDown;
      break;
    case ui::ET_KEY_RELEASED:
      type = WebKit::WebInputEvent::KeyUp;
      break;
    default:
      NOTREACHED();
  }

  modifiers = EventFlagsToWebInputEventModifiers(state);
  timeStampSeconds = time_stamp_seconds;
  windowsKeyCode = character;
  nativeKeyCode = character;
  text[0] = character;
  unmodifiedText[0] = character;
  isSystemKey = (state & ui::EF_ALT_DOWN) != 0;
}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  WebKeyboardEvent::operator=(other);
  delete os_event;
  os_event = CopyEvent(other.os_event);
  skip_in_browser = other.skip_in_browser;

  return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {
  delete os_event;
}

}  // namespace content
