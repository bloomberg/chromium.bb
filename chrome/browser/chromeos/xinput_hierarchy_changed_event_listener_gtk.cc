// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/xinput_hierarchy_changed_event_listener.h"

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

namespace chromeos {

void XInputHierarchyChangedEventListener::Init() {
  gdk_window_add_filter(NULL, GdkEventFilter, this);
}

void XInputHierarchyChangedEventListener::StopImpl() {
  gdk_window_remove_filter(NULL, GdkEventFilter, this);
}

// static
GdkFilterReturn XInputHierarchyChangedEventListener::GdkEventFilter(
    GdkXEvent* gxevent, GdkEvent* gevent, gpointer data) {
  XInputHierarchyChangedEventListener* listener =
      static_cast<XInputHierarchyChangedEventListener*>(data);
  XEvent* xevent = static_cast<XEvent*>(gxevent);

  if (xevent->xcookie.type != GenericEvent)
    return GDK_FILTER_CONTINUE;

  if (!XGetEventData(xevent->xgeneric.display, &xevent->xcookie)) {
    VLOG(1) << "XGetEventData failed";
    return GDK_FILTER_CONTINUE;
  }
  bool processed = listener->ProcessedXEvent(xevent);
  XFreeEventData(xevent->xgeneric.display, &xevent->xcookie);
  return  processed ? GDK_FILTER_REMOVE : GDK_FILTER_CONTINUE;
}

}  // namespace chromeos
