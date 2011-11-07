// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/xinput_hierarchy_changed_event_listener.h"

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "ui/base/x/x11_util.h"

namespace {

// Gets the major opcode for XInput2. Returns -1 on error.
int GetXInputOpCode() {
  static const char kExtensionName[] = "XInputExtension";
  int xi_opcode = -1;
  int event;
  int error;

  if (!XQueryExtension(
          ui::GetXDisplay(), kExtensionName, &xi_opcode, &event, &error)) {
    VLOG(1) << "X Input extension not available: error=" << error;
    return -1;
  }
  return xi_opcode;
}

// Starts listening to the XI_HierarchyChanged events.
void SelectXInputEvents() {
  XIEventMask evmask;
  unsigned char mask[XIMaskLen(XI_LASTEVENT)] = {};
  XISetMask(mask, XI_HierarchyChanged);

  evmask.deviceid = XIAllDevices;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;

  Display* display = ui::GetXDisplay();
  XISelectEvents(display, DefaultRootWindow(display), &evmask, 1);
}

// Checks the |event| and asynchronously sets the XKB layout when necessary.
void HandleHierarchyChangedEvent(XIHierarchyEvent* event) {
  if (!(event->flags & XISlaveAdded)) {
    return;
  }
  for (int i = 0; i < event->num_info; ++i) {
    XIHierarchyInfo* info = &event->info[i];
    if ((info->flags & XISlaveAdded) && (info->use == XIFloatingSlave)) {
      chromeos::input_method::InputMethodManager* input_method_manager =
          chromeos::input_method::InputMethodManager::GetInstance();
      chromeos::input_method::XKeyboard* xkeyboard =
          input_method_manager->GetXKeyboard();
      xkeyboard->ReapplyCurrentModifierLockStatus();
      xkeyboard->ReapplyCurrentKeyboardLayout();
      break;
    }
  }
}

}  // namespace

namespace chromeos {

// static
XInputHierarchyChangedEventListener*
XInputHierarchyChangedEventListener::GetInstance() {
  return Singleton<XInputHierarchyChangedEventListener>::get();
}

XInputHierarchyChangedEventListener::XInputHierarchyChangedEventListener()
    : stopped_(false),
      xiopcode_(GetXInputOpCode()) {
  SelectXInputEvents();

#if defined(TOUCH_UI) || !defined(TOOLKIT_USES_GTK)
  MessageLoopForUI::current()->AddObserver(this);
#else
  gdk_window_add_filter(NULL, GdkEventFilter, this);
#endif
}

XInputHierarchyChangedEventListener::~XInputHierarchyChangedEventListener() {
  Stop();
}

void XInputHierarchyChangedEventListener::Stop() {
  if (stopped_)
    return;

#if defined(TOUCH_UI) || !defined(TOOLKIT_USES_GTK)
  MessageLoopForUI::current()->RemoveObserver(this);
#else
  gdk_window_remove_filter(NULL, GdkEventFilter, this);
#endif
  stopped_ = true;
  xiopcode_ = -1;
}

#if defined(TOUCH_UI) || !defined(TOOLKIT_USES_GTK)
base::EventStatus XInputHierarchyChangedEventListener::WillProcessEvent(
    const base::NativeEvent& event) {
  // There may be multiple listeners for the XI_HierarchyChanged event. So
  // always return EVENT_CONTINUE to make sure all the listeners receive the
  // event.
  ProcessedXEvent(event);
  return base::EVENT_CONTINUE;
}

void XInputHierarchyChangedEventListener::DidProcessEvent(
    const base::NativeEvent& event) {
}
#else  // defined(TOUCH_UI) || !defined(TOOLKIT_USES_GTK)
// static
GdkFilterReturn XInputHierarchyChangedEventListener::GdkEventFilter(
    GdkXEvent* gxevent, GdkEvent* gevent, gpointer data) {
  XInputHierarchyChangedEventListener* listener =
      static_cast<XInputHierarchyChangedEventListener*>(data);
  XEvent* xevent = static_cast<XEvent*>(gxevent);

  return listener->ProcessedXEvent(xevent) ? GDK_FILTER_REMOVE
                                           : GDK_FILTER_CONTINUE;
}
#endif  // defined(TOUCH_UI) || !defined(TOOLKIT_USES_GTK)

bool XInputHierarchyChangedEventListener::ProcessedXEvent(XEvent* xevent) {
  if ((xevent->xcookie.type != GenericEvent) ||
      (xevent->xcookie.extension != xiopcode_)) {
    return false;
  }
#if !defined(TOUCH_UI)
  if (!XGetEventData(xevent->xgeneric.display, &xevent->xcookie)) {
    VLOG(1) << "XGetEventData failed";
    return false;
  }
#endif
  XGenericEventCookie* cookie = &(xevent->xcookie);
  const bool should_consume = (cookie->evtype == XI_HierarchyChanged);
  if (should_consume)
    HandleHierarchyChangedEvent(static_cast<XIHierarchyEvent*>(cookie->data));

#if !defined(TOUCH_UI)
  XFreeEventData(xevent->xgeneric.display, cookie);
#endif
  return should_consume;
}

}  // namespace chromeos
