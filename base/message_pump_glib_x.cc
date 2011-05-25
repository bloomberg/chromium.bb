// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_glib_x.h"

#include <gdk/gdkx.h>
#if defined(HAVE_XINPUT2)
#include <X11/extensions/XInput2.h>
#else
#include <X11/Xlib.h>
#endif

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
#if defined(HAVE_XINPUT2)
    xiopcode_(-1),
#endif
    gdksource_(NULL),
    dispatching_event_(false),
    capture_x_events_(0),
    capture_gdk_events_(0) {
  gdk_window_add_filter(NULL, &GdkEventFilter, this);
  gdk_event_handler_set(&EventDispatcherX, this, NULL);

#if defined(HAVE_XINPUT2)
  InitializeXInput2();
#endif
  InitializeEventsToCapture();
}

MessagePumpGlibX::~MessagePumpGlibX() {
}

bool MessagePumpGlibX::ShouldCaptureXEvent(XEvent* xev) {
  return capture_x_events_[xev->type]
#if defined(HAVE_XINPUT2)
        && (xev->type != GenericEvent || xev->xcookie.extension == xiopcode_)
#endif
    ;
}


bool MessagePumpGlibX::ProcessXEvent(XEvent* xev) {
  bool should_quit = false;

#if defined(HAVE_XINPUT2)
  bool have_cookie = false;
  if (xev->type == GenericEvent &&
      XGetEventData(xev->xgeneric.display, &xev->xcookie)) {
    have_cookie = true;
  }
#endif

  if (!WillProcessXEvent(xev)) {
    MessagePumpGlibXDispatcher::DispatchStatus status =
      static_cast<MessagePumpGlibXDispatcher*>
      (GetDispatcher())->DispatchX(xev);

    if (status == MessagePumpGlibXDispatcher::EVENT_QUIT) {
      should_quit = true;
      Quit();
    } else if (status == MessagePumpGlibXDispatcher::EVENT_IGNORED) {
      DLOG(WARNING) << "Event (" << xev->type << ") not handled.";
    }
  }

#if defined(HAVE_XINPUT2)
  if (have_cookie) {
    XFreeEventData(xev->xgeneric.display, &xev->xcookie);
  }
#endif

  return should_quit;
}

bool MessagePumpGlibX::RunOnce(GMainContext* context, bool block) {
  GdkDisplay* gdisp = gdk_display_get_default();
  if (!gdisp || !GetDispatcher())
    return MessagePumpForUI::RunOnce(context, block);

  Display* display = GDK_DISPLAY_XDISPLAY(gdisp);

  if (XPending(display)) {
    XEvent xev;
    XPeekEvent(display, &xev);

    if (ShouldCaptureXEvent(&xev)) {
      XNextEvent(display, &xev);
      if (ProcessXEvent(&xev))
        return true;
    } else {
      // TODO(sad): A couple of extra events can still sneak in during this.
      // Those should be sent back to the X queue from the dispatcher
      // EventDispatcherX.
      if (gdksource_)
        gdksource_->source_funcs->dispatch = gdkdispatcher_;
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

GdkFilterReturn MessagePumpGlibX::GdkEventFilter(GdkXEvent* gxevent,
                                                 GdkEvent* gevent,
                                                 gpointer data) {
  MessagePumpGlibX* pump = static_cast<MessagePumpGlibX*>(data);
  XEvent* xev = static_cast<XEvent*>(gxevent);

  if (pump->ShouldCaptureXEvent(xev) && pump->GetDispatcher()) {
    pump->ProcessXEvent(xev);
    return GDK_FILTER_REMOVE;
  }

  return GDK_FILTER_CONTINUE;
}

bool MessagePumpGlibX::WillProcessXEvent(XEvent* xevent) {
  ObserverListBase<Observer>::Iterator it(observers());
  Observer* obs;
  while ((obs = it.GetNext()) != NULL) {
    MessagePumpXObserver* xobs =
        static_cast<MessagePumpXObserver*>(obs);
    if (xobs->WillProcessXEvent(xevent))
      return true;
  }
  return false;
}

void MessagePumpGlibX::EventDispatcherX(GdkEvent* event, gpointer data) {
  MessagePumpGlibX* pump_x = reinterpret_cast<MessagePumpGlibX*>(data);

  if (!pump_x->gdksource_) {
    pump_x->gdksource_ = g_main_current_source();
    if (pump_x->gdksource_)
      pump_x->gdkdispatcher_ = pump_x->gdksource_->source_funcs->dispatch;
  } else if (!pump_x->IsDispatchingEvent()) {
    if (event->type != GDK_NOTHING &&
        pump_x->capture_gdk_events_[event->type]) {
      NOTREACHED() << "GDK received an event it shouldn't have";
    }
  }

  pump_x->DispatchEvents(event);
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

#if defined(HAVE_XINPUT2)
  capture_x_events_[GenericEvent] = true;
#endif
}

#if defined(HAVE_XINPUT2)
void MessagePumpGlibX::InitializeXInput2(void) {
  GdkDisplay* display = gdk_display_get_default();
  if (!display)
    return;

  Display* xdisplay = GDK_DISPLAY_XDISPLAY(display);
  int event, err;

  if (!XQueryExtension(xdisplay, "XInputExtension", &xiopcode_, &event, &err)) {
    DLOG(WARNING) << "X Input extension not available.";
    xiopcode_ = -1;
    return;
  }

  int major = 2, minor = 0;
  if (XIQueryVersion(xdisplay, &major, &minor) == BadRequest) {
    DLOG(WARNING) << "XInput2 not supported in the server.";
    xiopcode_ = -1;
    return;
  }
}
#endif  // HAVE_XINPUT2

bool MessagePumpXObserver::WillProcessXEvent(XEvent* xev) {
  return false;
}

}  // namespace base
