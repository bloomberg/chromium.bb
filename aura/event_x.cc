// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "aura/event.h"

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

#include "base/logging.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"

namespace aura {

namespace {

int GetEventFlagsFromXState(unsigned int state) {
  int flags = 0;
  if (state & ControlMask)
    flags |= ui::EF_CONTROL_DOWN;
  if (state & ShiftMask)
    flags |= ui::EF_SHIFT_DOWN;
  if (state & Mod1Mask)
    flags |= ui::EF_ALT_DOWN;
  if (state & LockMask)
    flags |= ui::EF_CAPS_LOCK_DOWN;
  if (state & Button1Mask)
    flags |= ui::EF_LEFT_BUTTON_DOWN;
  if (state & Button2Mask)
    flags |= ui::EF_MIDDLE_BUTTON_DOWN;
  if (state & Button3Mask)
    flags |= ui::EF_RIGHT_BUTTON_DOWN;

  return flags;
}

// Get the event flag for the button in XButtonEvent. During a ButtonPress
// event, |state| in XButtonEvent does not include the button that has just been
// pressed. Instead |state| contains flags for the buttons (if any) that had
// already been pressed before the current button, and |button| stores the most
// current pressed button. So, if you press down left mouse button, and while
// pressing it down, press down the right mouse button, then for the latter
// event, |state| would have Button1Mask set but not Button3Mask, and |button|
// would be 3.
int GetEventFlagsForButton(int button) {
  switch (button) {
    case 1:
      return ui::EF_LEFT_BUTTON_DOWN;
    case 2:
      return ui::EF_MIDDLE_BUTTON_DOWN;
    case 3:
      return ui::EF_RIGHT_BUTTON_DOWN;
  }

  DLOG(WARNING) << "Unexpected button (" << button << ") received.";
  return 0;
}

int GetButtonMaskForX2Event(XIDeviceEvent* xievent) {
  int buttonflags = 0;

  for (int i = 0; i < 8 * xievent->buttons.mask_len; i++) {
    if (XIMaskIsSet(xievent->buttons.mask, i)) {
      buttonflags |= GetEventFlagsForButton(i);
    }
  }

  return buttonflags;
}

ui::EventType EventTypeFromNative(NativeEvent native_event) {
  switch (native_event->type) {
    case KeyPress:
      return ui::ET_KEY_PRESSED;
    case KeyRelease:
      return ui::ET_KEY_RELEASED;
    case ButtonPress:
      if (native_event->xbutton.button == 4 ||
          native_event->xbutton.button == 5)
        return ui::ET_MOUSEWHEEL;
      return ui::ET_MOUSE_PRESSED;
    case ButtonRelease:
      if (native_event->xbutton.button == 4 ||
          native_event->xbutton.button == 5)
        return ui::ET_MOUSEWHEEL;
      return ui::ET_MOUSE_RELEASED;
    case MotionNotify:
      if (native_event->xmotion.state &
          (Button1Mask | Button2Mask | Button3Mask))
        return ui::ET_MOUSE_DRAGGED;
      return ui::ET_MOUSE_MOVED;
    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(native_event->xcookie.data);
      // TODO(sad): Determine if sourceid is a touch device.
      switch (xievent->evtype) {
        case XI_ButtonPress:
          return (xievent->detail == 4 || xievent->detail == 5) ?
              ui::ET_MOUSEWHEEL : ui::ET_MOUSE_PRESSED;
        case XI_ButtonRelease:
          return (xievent->detail == 4 || xievent->detail == 5) ?
              ui::ET_MOUSEWHEEL : ui::ET_MOUSE_RELEASED;
        case XI_Motion:
          return GetButtonMaskForX2Event(xievent) ? ui::ET_MOUSE_DRAGGED :
              ui::ET_MOUSE_MOVED;
      }
    }
    default:
      NOTREACHED();
      break;
  }
  return ui::ET_UNKNOWN;
}

gfx::Point GetEventLocation(XEvent* xev) {
  switch (xev->type) {
    case ButtonPress:
    case ButtonRelease:
      return gfx::Point(xev->xbutton.x, xev->xbutton.y);

    case MotionNotify:
      return gfx::Point(xev->xmotion.x, xev->xmotion.y);

    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(xev->xcookie.data);
      return gfx::Point(static_cast<int>(xievent->event_x),
                        static_cast<int>(xievent->event_y));
    }
  }

  return gfx::Point();
}

int GetLocatedEventFlags(XEvent* xev) {
  switch (xev->type) {
    case ButtonPress:
    case ButtonRelease:
      return GetEventFlagsFromXState(xev->xbutton.state) |
             GetEventFlagsForButton(xev->xbutton.button);

    case MotionNotify:
      return GetEventFlagsFromXState(xev->xmotion.state);

    case GenericEvent: {
      XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev->xcookie.data);
      bool touch = false;  // TODO(sad): Determine if xievent->sourceid is a
                           //            touch device.
      switch (xievent->evtype) {
        case XI_ButtonPress:
        case XI_ButtonRelease:
          return GetButtonMaskForX2Event(xievent) |
                 GetEventFlagsFromXState(xievent->mods.effective) |
                 (touch ? 0 : GetEventFlagsForButton(xievent->detail));

        case XI_Motion:
           return GetButtonMaskForX2Event(xievent) |
                  GetEventFlagsFromXState(xievent->mods.effective);
      }
    }
  }

  return 0;
}

}  // namespace

void Event::Init() {
  memset(&native_event_, 0, sizeof(native_event_));
}

void Event::InitWithNativeEvent(NativeEvent native_event) {
  native_event_ = native_event;
}

LocatedEvent::LocatedEvent(NativeEvent native_event)
    : Event(native_event, EventTypeFromNative(native_event),
            GetLocatedEventFlags(native_event)),
      location_(GetEventLocation(native_event)) {
}

MouseEvent::MouseEvent(NativeEvent native_event)
    : LocatedEvent(native_event) {
}

KeyEvent::KeyEvent(NativeEvent native_event)
    : Event(native_event,
            EventTypeFromNative(native_event),
            GetEventFlagsFromXState(native_event->xbutton.state)),
      key_code_(ui::KeyboardCodeFromXKeyEvent(native_event)) {
}

}  // namespace aura
