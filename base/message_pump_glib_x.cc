// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_glib_x.h"

#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "base/message_pump_glib_x_dispatch.h"

namespace {

gboolean PlaceholderDispatch(GSource* source,
                             GSourceFunc cb,
                             gpointer data) {
  return TRUE;
}

}  // namespace

namespace base {

MessagePumpGlibX::MessagePumpGlibX() : base::MessagePumpForUI(),
    gdksource_(NULL),
    dispatching_event_(false),
    capture_x_events_(0),
    capture_gdk_events_(0) {
  gdk_event_handler_set(&EventDispatcherX, this, NULL);

  InitializeEventsToCapture();
}

MessagePumpGlibX::~MessagePumpGlibX() {
}

bool MessagePumpGlibX::RunOnce(GMainContext* context, bool block) {
  GdkDisplay* gdisp = gdk_display_get_default();
  Display* display = GDK_DISPLAY_XDISPLAY(gdisp);
  if (XPending(display)) {
    XEvent xev;
    XPeekEvent(display, &xev);
    if (capture_x_events_[xev.type]) {
      XNextEvent(display, &xev);

      bool processed = static_cast<MessagePumpGlibXDispatcher*>
          (GetDispatcher())->Dispatch(&xev);

      if (!processed) {
        DLOG(WARNING) << "Event (" << xev.type << ") not handled.";
      }
    } else {
      // TODO(sad): A couple of extra events can still sneak in during this.
      // Those should be sent back to the X queue from the dispatcher
      // EventDispatcherX.
      g_main_context_iteration(context, FALSE);
    }
  }

  bool retvalue;
  if (gdksource_) {
    // Replace the dispatch callback of the GDK event source temporarily so that
    // it doesn't read events from X.
    gboolean (*cb)(GSource*, GSourceFunc, void*) =
        gdksource_->source_funcs->dispatch;
    gdksource_->source_funcs->dispatch = PlaceholderDispatch;

    dispatching_event_ = true;
    retvalue = g_main_context_iteration(context, block);
    dispatching_event_ = false;

    gdksource_->source_funcs->dispatch = cb;
  } else {
    retvalue = g_main_context_iteration(context, block);
  }

  return retvalue;
}

void MessagePumpGlibX::InitializeEventsToCapture(void) {
  // TODO(sad): Decide which events we want to capture and update the tables
  // accordingly.
  capture_x_events_[KeyPress] = true;
  capture_gdk_events_[GDK_KEY_PRESS] = true;

  capture_x_events_[KeyRelease] = true;
  capture_gdk_events_[GDK_KEY_RELEASE] = true;

  capture_x_events_[ButtonPress] = true;
  capture_gdk_events_[GDK_BUTTON_PRESS] = true;

  capture_x_events_[ButtonRelease] = true;
  capture_gdk_events_[GDK_BUTTON_RELEASE] = true;

  capture_x_events_[MotionNotify] = true;
  capture_gdk_events_[GDK_MOTION_NOTIFY] = true;
}

void MessagePumpGlibX::EventDispatcherX(GdkEvent* event, gpointer data) {
  MessagePumpGlibX* pump_x = reinterpret_cast<MessagePumpGlibX*>(data);

  if (!pump_x->gdksource_) {
    pump_x->gdksource_ = g_main_current_source();
  } else if (!pump_x->IsDispatchingEvent()) {
    if (event->type != GDK_NOTHING &&
        pump_x->capture_gdk_events_[event->type]) {
      // TODO(sad): An X event is caught by the GDK handler. Put it back in the
      // X queue so that we catch it in the next iteration. When done, the
      // following DLOG statement will be removed.
      DLOG(WARNING) << "GDK received an event it shouldn't have";
    }
  }

  pump_x->DispatchEvents(event);
}

}  // namespace base
