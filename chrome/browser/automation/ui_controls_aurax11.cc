// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/ui_controls.h"

#include <X11/keysym.h>
#include <X11/Xlib.h>

#include "base/callback.h"
#include "base/logging.h"
#include "base/message_pump_x.h"
#include "chrome/browser/automation/ui_controls_internal.h"
#include "ui/aura/desktop.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "views/view.h"

namespace {

// Event waiter executes the specified closure|when a matching event
// is found.
// TODO(oshima): Move this to base.
class EventWaiter : public MessageLoopForUI::Observer {
 public:
  typedef bool (*EventWaiterMatcher)(const base::NativeEvent& event);

  EventWaiter(const base::Closure& closure, EventWaiterMatcher matcher)
      : closure_(closure),
        matcher_(matcher) {
    MessageLoopForUI::current()->AddObserver(this);
  }

  virtual ~EventWaiter() {
    MessageLoopForUI::current()->RemoveObserver(this);
  }

  // MessageLoop::Observer implementation:
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE {
    if ((*matcher_)(event)) {
      MessageLoop::current()->PostTask(FROM_HERE, closure_);
      delete this;
    }
    return base::EVENT_CONTINUE;
  }

  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE {
  }

 private:
  base::Closure closure_;
  EventWaiterMatcher matcher_;
  DISALLOW_COPY_AND_ASSIGN(EventWaiter);
};

// Latest mouse pointer location set by SendMouseMoveNotifyWhenDone.
int g_current_x = -1000;
int g_current_y = -1000;

// Returns atom that indidates that the XEvent is marker event.
Atom MarkerEventAtom() {
  return XInternAtom(base::MessagePumpX::GetDefaultXDisplay(),
                     "marker_event",
                     False);
}

// Returns true when the event is a marker event.
bool Matcher(const base::NativeEvent& event) {
  return event->xany.type == ClientMessage &&
      event->xclient.message_type == MarkerEventAtom();
}

void RunClosureAfterEvents(const base::Closure closure) {
  if (closure.is_null())
    return;
  static XEvent* marker_event = NULL;
  if (!marker_event) {
    marker_event = new XEvent();
    marker_event->xclient.type = ClientMessage;
    marker_event->xclient.display = NULL;
    marker_event->xclient.window = None;
    marker_event->xclient.format = 8;
  }
  marker_event->xclient.message_type = MarkerEventAtom();
  aura::Desktop::GetInstance()->PostNativeEvent(marker_event);
  new EventWaiter(closure, &Matcher);
}

}  // namespace

namespace ui_controls {

bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command) {
  DCHECK(!command);  // No command key on Aura
  return SendKeyPressNotifyWhenDone(
      window, key, control, shift, alt, command, base::Closure());
}

void SetMaskAndKeycodeThenSend(XEvent* xevent,
                               unsigned int mask,
                               unsigned int keycode) {
  xevent->xkey.state |= mask;
  xevent->xkey.keycode = keycode;
  aura::Desktop::GetInstance()->PostNativeEvent(xevent);
}

void SetKeycodeAndSendThenUnmask(XEvent* xevent,
                                 unsigned int mask,
                                 unsigned int keycode) {
  xevent->xkey.keycode = keycode;
  aura::Desktop::GetInstance()->PostNativeEvent(xevent);
  xevent->xkey.state ^= mask;
}

bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                const base::Closure& closure) {
  DCHECK(!command);  // No command key on Aura
  XEvent xevent = {0};
  xevent.xkey.type = KeyPress;
  if (control)
    SetMaskAndKeycodeThenSend(&xevent, ControlMask, XK_Control_L);
  if (shift)
    SetMaskAndKeycodeThenSend(&xevent, ShiftMask, XK_Shift_L);
  if (alt)
    SetMaskAndKeycodeThenSend(&xevent, Mod1Mask, XK_Alt_L);
  xevent.xkey.keycode = ui::XKeysymForWindowsKeyCode(key, shift);
  aura::Desktop::GetInstance()->PostNativeEvent(&xevent);

  // Send key release events.
  xevent.xkey.type = KeyRelease;
  aura::Desktop::GetInstance()->PostNativeEvent(&xevent);
  if (alt)
    SetKeycodeAndSendThenUnmask(&xevent, Mod1Mask, XK_Alt_L);
  if (shift)
    SetKeycodeAndSendThenUnmask(&xevent, ShiftMask, XK_Shift_L);
  if (control)
    SetKeycodeAndSendThenUnmask(&xevent, ControlMask, XK_Control_L);
  DCHECK(!xevent.xkey.state);
  RunClosureAfterEvents(closure);
  return true;
}

bool SendMouseMove(long x, long y) {
  return SendMouseMoveNotifyWhenDone(x, y, base::Closure());
}

bool SendMouseMoveNotifyWhenDone(long x, long y, const base::Closure& closure) {
  XEvent xevent = {0};
  XMotionEvent* xmotion = &xevent.xmotion;
  xmotion->type = MotionNotify;
  g_current_x = xmotion->x = x;
  g_current_y = xmotion->y = y;
  xmotion->same_screen = True;
  // Desktop will take care of other necessary fields.
  aura::Desktop::GetInstance()->PostNativeEvent(&xevent);
  RunClosureAfterEvents(closure);
  return false;
}

bool SendMouseEvents(MouseButton type, int state) {
  return SendMouseEventsNotifyWhenDone(type, state, base::Closure());
}

bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                   int state,
                                   const base::Closure& closure) {
  XEvent xevent = {0};
  XButtonEvent* xbutton = &xevent.xbutton;
  DCHECK_NE(g_current_x, -1000);
  DCHECK_NE(g_current_y, -1000);
  xbutton->x = g_current_x;
  xbutton->y = g_current_y;
  xbutton->same_screen = True;
  switch (type) {
    case LEFT:
      xbutton->button = Button1;
      xbutton->state = Button1Mask;
      break;
    case MIDDLE:
      xbutton->button = Button2;
      xbutton->state = Button2Mask;
      break;
    case RIGHT:
      xbutton->button = Button3;
      xbutton->state = Button3Mask;
      break;
  }
  // Desktop will take care of other necessary fields.

  aura::Desktop* desktop = aura::Desktop::GetInstance();
  if (state & DOWN) {
    xevent.xbutton.type = ButtonPress;
    desktop->PostNativeEvent(&xevent);
  }
  if (state & UP) {
    xevent.xbutton.type = ButtonRelease;
    desktop->PostNativeEvent(&xevent);
  }
  RunClosureAfterEvents(closure);
  return false;
}

bool SendMouseClick(MouseButton type) {
  return SendMouseEvents(type, UP | DOWN);
}

void MoveMouseToCenterAndPress(views::View* view, MouseButton button,
                               int state, const base::Closure& closure) {
  DCHECK(view);
  DCHECK(view->GetWidget());
  gfx::Point view_center(view->width() / 2, view->height() / 2);
  views::View::ConvertPointToScreen(view, &view_center);
  SendMouseMove(view_center.x(), view_center.y());
  SendMouseEventsNotifyWhenDone(button, state, closure);
}

}  // namespace ui_controls
