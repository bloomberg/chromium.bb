// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_input_event_aura.h"

#include "ui/aura/window.h"
#include "ui/base/events/event.h"

namespace content {

#if defined(OS_WIN)
WebKit::WebMouseEvent MakeUntranslatedWebMouseEventFromNativeEvent(
    base::NativeEvent native_event);
WebKit::WebMouseWheelEvent MakeUntranslatedWebMouseWheelEventFromNativeEvent(
    base::NativeEvent native_event);
WebKit::WebKeyboardEvent MakeWebKeyboardEventFromNativeEvent(
    base::NativeEvent native_event);
WebKit::WebGestureEvent MakeWebGestureEventFromNativeEvent(
    base::NativeEvent native_event);
WebKit::WebTouchPoint* UpdateWebTouchEventFromNativeEvent(
    base::NativeEvent native_event, WebKit::WebTouchEvent* web_event);
#else
WebKit::WebMouseEvent MakeWebMouseEventFromAuraEvent(ui::MouseEvent* event);
WebKit::WebMouseWheelEvent MakeWebMouseWheelEventFromAuraEvent(
    ui::MouseWheelEvent* event);
WebKit::WebMouseWheelEvent MakeWebMouseWheelEventFromAuraEvent(
    ui::ScrollEvent* event);
WebKit::WebKeyboardEvent MakeWebKeyboardEventFromAuraEvent(
    ui::KeyEvent* event);
WebKit::WebGestureEvent MakeWebGestureEventFromAuraEvent(
    ui::GestureEvent* event);
WebKit::WebGestureEvent MakeWebGestureEventFromAuraEvent(
    ui::ScrollEvent* event);
WebKit::WebTouchPoint* UpdateWebTouchEventFromAuraEvent(
    ui::TouchEvent* event, WebKit::WebTouchEvent* web_event);
#endif

// General approach:
//
// ui::Event only carries a subset of possible event data provided to Aura by
// the host platform. WebKit utilizes a larger subset of that information than
// Aura itself. WebKit includes some built in cracking functionality that we
// rely on to obtain this information cleanly and consistently.
//
// The only place where an ui::Event's data differs from what the underlying
// base::NativeEvent would provide is position data, since we would like to
// provide coordinates relative to the aura::Window that is hosting the
// renderer, not the top level platform window.
//
// The approach is to fully construct a WebKit::WebInputEvent from the
// ui::Event's base::NativeEvent, and then replace the coordinate fields with
// the translated values from the ui::Event.
//
// The exception is mouse events on linux. The ui::MouseEvent contains enough
// necessary information to construct a WebMouseEvent. So instead of extracting
// the information from the XEvent, which can be tricky when supporting both
// XInput2 and XInput, the WebMouseEvent is constructed from the
// ui::MouseEvent. This will not be necessary once only XInput2 is supported.
//

WebKit::WebMouseEvent MakeWebMouseEvent(ui::MouseEvent* event) {
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

  const gfx::Point root_point = event->root_location();
  webkit_event.globalX = root_point.x();
  webkit_event.globalY = root_point.y();

  return webkit_event;
}

WebKit::WebMouseWheelEvent MakeWebMouseWheelEvent(ui::MouseWheelEvent* event) {
#if defined(OS_WIN)
  // Construct an untranslated event from the platform event data.
  WebKit::WebMouseWheelEvent webkit_event =
      MakeUntranslatedWebMouseWheelEventFromNativeEvent(event->native_event());
#else
  WebKit::WebMouseWheelEvent webkit_event =
      MakeWebMouseWheelEventFromAuraEvent(event);
#endif

  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event->x();
  webkit_event.windowY = webkit_event.y = event->y();

  const gfx::Point root_point = event->root_location();
  webkit_event.globalX = root_point.x();
  webkit_event.globalY = root_point.y();

  return webkit_event;
}

WebKit::WebMouseWheelEvent MakeWebMouseWheelEvent(ui::ScrollEvent* event) {
#if defined(OS_WIN)
  // Construct an untranslated event from the platform event data.
  WebKit::WebMouseWheelEvent webkit_event =
      MakeUntranslatedWebMouseWheelEventFromNativeEvent(event->native_event());
#else
  WebKit::WebMouseWheelEvent webkit_event =
      MakeWebMouseWheelEventFromAuraEvent(event);
#endif

  // Replace the event's coordinate fields with translated position data from
  // |event|.
  webkit_event.windowX = webkit_event.x = event->x();
  webkit_event.windowY = webkit_event.y = event->y();

  const gfx::Point root_point = event->root_location();
  webkit_event.globalX = root_point.x();
  webkit_event.globalY = root_point.y();

  return webkit_event;
}

WebKit::WebKeyboardEvent MakeWebKeyboardEvent(ui::KeyEvent* event) {
  // Windows can figure out whether or not to construct a RawKeyDown or a Char
  // WebInputEvent based on the type of message carried in
  // event->native_event(). X11 is not so fortunate, there is no separate
  // translated event type, so DesktopHostLinux sends an extra KeyEvent with
  // is_char() == true. We need to pass the ui::KeyEvent to the X11 function
  // to detect this case so the right event type can be constructed.
#if defined(OS_WIN)
  // Key events require no translation by the aura system.
  return MakeWebKeyboardEventFromNativeEvent(event->native_event());
#else
  return MakeWebKeyboardEventFromAuraEvent(event);
#endif
}

WebKit::WebGestureEvent MakeWebGestureEvent(ui::GestureEvent* event) {
  WebKit::WebGestureEvent gesture_event;
#if defined(OS_WIN)
  gesture_event = MakeWebGestureEventFromNativeEvent(event->native_event());
#else
  gesture_event = MakeWebGestureEventFromAuraEvent(event);
#endif

  gesture_event.x = event->x();
  gesture_event.y = event->y();

  const gfx::Point root_point = event->root_location();
  gesture_event.globalX = root_point.x();
  gesture_event.globalY = root_point.y();

  return gesture_event;
}

WebKit::WebGestureEvent MakeWebGestureEvent(ui::ScrollEvent* event) {
  WebKit::WebGestureEvent gesture_event;

#if defined(OS_WIN)
  gesture_event = MakeWebGestureEventFromNativeEvent(event->native_event());
#else
  gesture_event = MakeWebGestureEventFromAuraEvent(event);
#endif

  gesture_event.x = event->x();
  gesture_event.y = event->y();

  const gfx::Point root_point = event->root_location();
  gesture_event.globalX = root_point.x();
  gesture_event.globalY = root_point.y();

  return gesture_event;
}

WebKit::WebGestureEvent MakeWebGestureEventFlingCancel() {
  WebKit::WebGestureEvent gesture_event;

  // All other fields are ignored on a GestureFlingCancel event.
  gesture_event.type = WebKit::WebInputEvent::GestureFlingCancel;
  return gesture_event;
}

WebKit::WebTouchPoint* UpdateWebTouchEvent(ui::TouchEvent* event,
                                           WebKit::WebTouchEvent* web_event) {
#if defined(OS_WIN)
  return UpdateWebTouchEventFromNativeEvent(event->native_event(), web_event);
#else
  return UpdateWebTouchEventFromAuraEvent(event, web_event);
#endif
}

}  // namespace content
