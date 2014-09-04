// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions based heavily on:
// third_party/WebKit/public/web/gtk/WebInputEventFactory.cpp
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

#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdlib>

#include "base/event_types.h"
#include "base/logging.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {

// chromium WebKit does not provide a WebInputEventFactory for X11, so we have
// to do the work here ourselves.

blink::WebKeyboardEvent MakeWebKeyboardEventFromAuraEvent(
    ui::KeyEvent* event) {
  const base::NativeEvent& native_event = event->native_event();
  blink::WebKeyboardEvent webkit_event;
  XKeyEvent* native_key_event = &native_event->xkey;

  webkit_event.timeStampSeconds = event->time_stamp().InSecondsF();
  webkit_event.modifiers = EventFlagsToWebEventModifiers(event->flags());

  switch (native_event->type) {
    case KeyPress:
      webkit_event.type = event->is_char() ? blink::WebInputEvent::Char :
          blink::WebInputEvent::RawKeyDown;
      break;
    case KeyRelease:
      webkit_event.type = blink::WebInputEvent::KeyUp;
      break;
    default:
      NOTREACHED();
  }

  if (webkit_event.modifiers & blink::WebInputEvent::AltKey)
    webkit_event.isSystemKey = true;

  webkit_event.windowsKeyCode = ui::WindowsKeycodeFromNative(native_event);
  webkit_event.nativeKeyCode = native_key_event->keycode;
  webkit_event.unmodifiedText[0] =
      ui::UnmodifiedTextFromNative(native_event);
  webkit_event.text[0] = ui::TextFromNative(native_event);

  webkit_event.setKeyIdentifierFromWindowsKeyCode();

  // TODO: IsKeyPad?

  return webkit_event;
}

}  // namespace content
