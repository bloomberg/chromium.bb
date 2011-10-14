// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions based heavily on:
// third_party/WebKit/Source/WebKit/chromium/public/gtk/WebInputEventFactory.cpp
//
/*
 * Copyright (C) 2006-2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "content/browser/renderer_host/web_input_event_aura.h"

#include <cstdlib>
#include <X11/Xlib.h>

#include "base/event_types.h"
#include "base/logging.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace content {

// chromium WebKit does not provide a WebInputEventFactory for X11, so we have
// to do the work here ourselves.

namespace {

double XEventTimeToWebEventTime(Time time) {
  // Convert from time in ms to time in s.
  return time / 1000.0;
}

int XStateToWebEventModifiers(unsigned int state) {
  int modifiers = 0;
  if (state & ShiftMask)
    modifiers |= WebKit::WebInputEvent::ShiftKey;
  if (state & ControlMask)
    modifiers |= WebKit::WebInputEvent::ControlKey;
  if (state & Mod1Mask)
    modifiers |= WebKit::WebInputEvent::AltKey;
  // TODO(beng): MetaKey/META_MASK
  if (state & Button1Mask)
    modifiers |= WebKit::WebInputEvent::LeftButtonDown;
  if (state & Button2Mask)
    modifiers |= WebKit::WebInputEvent::MiddleButtonDown;
  if (state & Button3Mask)
    modifiers |= WebKit::WebInputEvent::RightButtonDown;
  if (state & LockMask)
    modifiers |= WebKit::WebInputEvent::CapsLockOn;
  if (state & Mod2Mask)
    modifiers |= WebKit::WebInputEvent::NumLockOn;
  return modifiers;
}

int XKeyEventToWindowsKeyCode(XKeyEvent* event) {
  // TODO(beng):
  return 0;
}

// From
// third_party/WebKit/Source/WebKit/chromium/src/gtk/WebInputEventFactory.cpp:
WebKit::WebUChar GetControlCharacter(int windows_key_code, bool shift) {
  if (windows_key_code >= ui::VKEY_A &&
    windows_key_code <= ui::VKEY_Z) {
    // ctrl-A ~ ctrl-Z map to \x01 ~ \x1A
    return windows_key_code - ui::VKEY_A + 1;
  }
  if (shift) {
    // following graphics chars require shift key to input.
    switch (windows_key_code) {
      // ctrl-@ maps to \x00 (Null byte)
      case ui::VKEY_2:
        return 0;
      // ctrl-^ maps to \x1E (Record separator, Information separator two)
      case ui::VKEY_6:
        return 0x1E;
      // ctrl-_ maps to \x1F (Unit separator, Information separator one)
      case ui::VKEY_OEM_MINUS:
        return 0x1F;
      // Returns 0 for all other keys to avoid inputting unexpected chars.
      default:
        break;
    }
  } else {
    switch (windows_key_code) {
      // ctrl-[ maps to \x1B (Escape)
      case ui::VKEY_OEM_4:
        return 0x1B;
      // ctrl-\ maps to \x1C (File separator, Information separator four)
      case ui::VKEY_OEM_5:
        return 0x1C;
      // ctrl-] maps to \x1D (Group separator, Information separator three)
      case ui::VKEY_OEM_6:
        return 0x1D;
      // ctrl-Enter maps to \x0A (Line feed)
      case ui::VKEY_RETURN:
        return 0x0A;
      // Returns 0 for all other keys to avoid inputting unexpected chars.
      default:
        break;
    }
  }
  return 0;
}

WebKit::WebMouseEvent::Button ButtonFromXButton(int button) {
  switch (button) {
    case 1:
      return WebKit::WebMouseEvent::ButtonLeft;
    case 2:
      return WebKit::WebMouseEvent::ButtonMiddle;
    case 3:
      return WebKit::WebMouseEvent::ButtonRight;
    default:
      break;
  }
  return WebKit::WebMouseEvent::ButtonNone;
}

WebKit::WebMouseEvent::Button ButtonFromXState(int state) {
  if (state & Button1MotionMask)
    return WebKit::WebMouseEvent::ButtonLeft;
  if (state & Button2MotionMask)
    return WebKit::WebMouseEvent::ButtonMiddle;
  if (state & Button3MotionMask)
    return WebKit::WebMouseEvent::ButtonRight;
  return WebKit::WebMouseEvent::ButtonNone;
}

// We have to count clicks (for double-clicks) manually.
unsigned int g_num_clicks = 0;
::Window* g_last_click_window = NULL;
Time g_last_click_time = 0;
int g_last_click_x = 0;
 int g_last_click_y = 0;
WebKit::WebMouseEvent::Button g_last_click_button =
    WebKit::WebMouseEvent::ButtonNone;

bool ShouldForgetPreviousClick(::Window* window, Time time, int x, int y) {
  if (window != g_last_click_window)
    return true;

  const Time double_click_time = 250;
  const int double_click_distance = 5;
  return (time - g_last_click_time) > double_click_time
      || std::abs(x - g_last_click_x) > double_click_distance
      || std::abs(y - g_last_click_y) > double_click_distance;
}

void ResetClickCountState() {
  g_num_clicks = 0;
  g_last_click_window = NULL;
  g_last_click_time = 0;
  g_last_click_x = 0;
  g_last_click_y = 0;
  g_last_click_button = WebKit::WebMouseEvent::ButtonNone;
}

void InitWebKitEventFromButtonEvent(WebKit::WebMouseEvent* webkit_event,
                                    XButtonEvent* native_event) {
  webkit_event->modifiers = XStateToWebEventModifiers(native_event->state);
  webkit_event->timeStampSeconds = XEventTimeToWebEventTime(native_event->time);

  webkit_event->x = native_event->x;
  webkit_event->y = native_event->y;
  webkit_event->windowX = webkit_event->x;
  webkit_event->windowY = webkit_event->y;
  webkit_event->globalX = native_event->x_root;
  webkit_event->globalY = native_event->y_root;

  switch (native_event->type) {
    case ButtonPress:
      webkit_event->type = WebKit::WebInputEvent::MouseDown;
      webkit_event->button = ButtonFromXButton(native_event->button);
      if (!ShouldForgetPreviousClick(&native_event->window,
                                    native_event->time,
                                    native_event->x,
                                    native_event->y) &&
          webkit_event->button == g_last_click_button) {
        ++g_num_clicks;
      } else {
        g_num_clicks = 1;
        g_last_click_window = &native_event->window;
        g_last_click_x = native_event->x;
        g_last_click_y = native_event->y;
        g_last_click_button = webkit_event->button;
      }
      break;
    case ButtonRelease:
      webkit_event->type = WebKit::WebInputEvent::MouseUp;
      webkit_event->button = ButtonFromXButton(native_event->button);
      break;
    default:
      NOTREACHED();
      break;
  }
  webkit_event->clickCount = g_num_clicks;
}

void InitWebKitEventFromMotionEvent(WebKit::WebMouseEvent* webkit_event,
                                    XMotionEvent* native_event) {
  webkit_event->modifiers = XStateToWebEventModifiers(native_event->state);
  webkit_event->timeStampSeconds = XEventTimeToWebEventTime(native_event->time);

  webkit_event->x = native_event->x;
  webkit_event->y = native_event->y;
  webkit_event->windowX = webkit_event->x;
  webkit_event->windowY = webkit_event->y;
  webkit_event->globalX = native_event->x_root;
  webkit_event->globalY = native_event->y_root;

  webkit_event->type = WebKit::WebInputEvent::MouseMove;
  webkit_event->button = ButtonFromXState(native_event->state);
  if (ShouldForgetPreviousClick(&native_event->window,
                                native_event->time,
                                native_event->x,
                                native_event->y)) {
    ResetClickCountState();
  }
}

}  // namespace

WebKit::WebMouseEvent MakeUntranslatedWebMouseEventFromNativeEvent(
    base::NativeEvent native_event) {
  WebKit::WebMouseEvent webkit_event;

  // In X, button and mouse movement events are different event types that
  // require different handling.
  // TODO(sadrul): Add support for XInput2 events.
  switch (native_event->type) {
    case ButtonPress:
    case ButtonRelease:
      InitWebKitEventFromButtonEvent(&webkit_event, &native_event->xbutton);
      break;
    case MotionNotify:
      InitWebKitEventFromMotionEvent(&webkit_event, &native_event->xmotion);
      break;
    default:
      NOTREACHED();
      break;
  }
  return webkit_event;
}

WebKit::WebKeyboardEvent MakeWebKeyboardEventFromNativeEvent(
    base::NativeEvent native_event) {
  WebKit::WebKeyboardEvent webkit_event;
  XKeyEvent* native_key_event = &native_event->xkey;

  webkit_event.timeStampSeconds =
      XEventTimeToWebEventTime(native_key_event->time);
  webkit_event.modifiers = XStateToWebEventModifiers(native_key_event->state);

  switch (native_event->type) {
    case KeyPress:
      webkit_event.type = WebKit::WebInputEvent::RawKeyDown;
      break;
    case KeyRelease:
      webkit_event.type = WebKit::WebInputEvent::KeyUp;
      break;
    default:
      NOTREACHED();
  }

  if (webkit_event.modifiers & WebKit::WebInputEvent::AltKey)
    webkit_event.isSystemKey = true;

  webkit_event.windowsKeyCode = XKeyEventToWindowsKeyCode(native_key_event);
  webkit_event.nativeKeyCode = native_key_event->keycode;

  if (webkit_event.windowsKeyCode == ui::VKEY_RETURN) {
    webkit_event.unmodifiedText[0] = '\r';
  } else {
    webkit_event.unmodifiedText[0] =
        static_cast<WebKit::WebUChar>(native_key_event->keycode);
  }

  if (webkit_event.modifiers & WebKit::WebInputEvent::ControlKey) {
    webkit_event.text[0] =
        GetControlCharacter(
            webkit_event.windowsKeyCode,
            webkit_event.modifiers & WebKit::WebInputEvent::ShiftKey);
  } else {
    webkit_event.text[0] = webkit_event.unmodifiedText[0];
  }

  webkit_event.setKeyIdentifierFromWindowsKeyCode();

  // TODO: IsAutoRepeat/IsKeyPad?

  return webkit_event;
}

}  // namespace content
