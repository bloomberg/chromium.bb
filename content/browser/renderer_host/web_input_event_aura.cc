// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_input_event_aura.h"

#include "ui/aura/event.h"

namespace content {

#if defined(OS_WIN)
WebKit::WebMouseEvent MakeUntranslatedWebMouseEventFromNativeEvent(
    base::NativeEvent native_event);
WebKit::WebKeyboardEvent MakeWebKeyboardEventFromNativeEvent(
    base::NativeEvent native_event);
#else
WebKit::WebMouseEvent MakeWebMouseEventFromAuraEvent(aura::MouseEvent* event);
WebKit::WebKeyboardEvent MakeWebKeyboardEventFromAuraEvent(
    aura::KeyEvent* event);
#endif

// General approach:
//
// aura::Event only carries a subset of possible event data provided to Aura by
// the host platform. WebKit utilizes a larger subset of that information than
// Aura itself. WebKit includes some built in cracking functionality that we
// rely on to obtain this information cleanly and consistently.
//
// The only place where an aura::Event's data differs from what the underlying
// base::NativeEvent would provide is position data, since we would like to
// provide coordinates relative to the aura::Window that is hosting the
// renderer, not the top level platform window.
//
// The approach is to fully construct a WebKit::WebInputEvent from the
// aura::Event's base::NativeEvent, and then replace the coordinate fields with
// the translated values from the aura::Event.
//
// The exception is mouse events on linux. The aura::MouseEvent contains enough
// necessary information to construct a WebMouseEvent. So instead of extracting
// the information from the XEvent, which can be tricky when supporting both
// XInput2 and XInput, the WebMouseEvent is constructed from the
// aura::MouseEvent. This will not be necessary once only XInput2 is supported.
//

WebKit::WebMouseEvent MakeWebMouseEvent(aura::MouseEvent* event) {
#if defined(OS_WIN)
  // Construct an untranslated event from the platform event data.
  WebKit::WebMouseEvent webkit_event =
      MakeUntranslatedWebMouseEventFromNativeEvent(event->native_event());
#else
  WebKit::WebMouseEvent webkit_event = MakeWebMouseEventFromAuraEvent(event);
#endif

  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event->x();
  webkit_event.windowY = webkit_event.y = event->y();

  // TODO(beng): map these to screen coordinates.
  webkit_event.globalX = event->x();
  webkit_event.globalY = event->y();

  return webkit_event;
}

WebKit::WebKeyboardEvent MakeWebKeyboardEvent(aura::KeyEvent* event) {
  // Windows can figure out whether or not to construct a RawKeyDown or a Char
  // WebInputEvent based on the type of message carried in
  // event->native_event(). X11 is not so fortunate, there is no separate
  // translated event type, so DesktopHostLinux sends an extra KeyEvent with
  // is_char() == true. We need to pass the aura::KeyEvent to the X11 function
  // to detect this case so the right event type can be constructed.
#if defined(OS_WIN)
  // Key events require no translation by the aura system.
  return MakeWebKeyboardEventFromNativeEvent(event->native_event());
#else
  return MakeWebKeyboardEventFromAuraEvent(event);
#endif
}

}  // namespace content
