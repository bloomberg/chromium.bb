// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_x.h"

#include <gdk/gdkx.h>
#include <X11/extensions/XInput2.h>

#include "base/basictypes.h"
#include "base/message_loop.h"

namespace {

gboolean XSourcePrepare(GSource* source, gint* timeout_ms) {
  if (XPending(base::MessagePumpX::GetDefaultXDisplay()))
    *timeout_ms = 0;
  else
    *timeout_ms = -1;
  return FALSE;
}

gboolean XSourceCheck(GSource* source) {
  return XPending(base::MessagePumpX::GetDefaultXDisplay());
}

gboolean XSourceDispatch(GSource* source,
                         GSourceFunc unused_func,
                         gpointer unused_data) {
  // TODO(sad): When GTK event proecssing is completely removed, the event
  // processing and dispatching should be done here (i.e. XNextEvent,
  // ProcessXEvent etc.)
  return TRUE;
}

GSourceFuncs XSourceFuncs = {
  XSourcePrepare,
  XSourceCheck,
  XSourceDispatch,
  NULL
};

// A flag to disable GTK's message pump. This is intermediate step
// to remove gtk and will be removed once migration is complete.
bool use_gtk_message_pump = true;

// The opcode used for checking events.
int xiopcode = -1;

// When the GTK/GDK event processing is disabled, the message-pump opens a
// connection to the display and owns it.
Display* g_xdisplay = NULL;

gboolean PlaceholderDispatch(GSource* source,
                             GSourceFunc cb,
                             gpointer data) {
  return TRUE;
}

void InitializeXInput2(void) {
  Display* display = base::MessagePumpX::GetDefaultXDisplay();
  if (!display)
    return;

  int event, err;

  if (!XQueryExtension(display, "XInputExtension", &xiopcode, &event, &err)) {
    VLOG(1) << "X Input extension not available.";
    xiopcode = -1;
    return;
  }

  int major = 2, minor = 0;
  if (XIQueryVersion(display, &major, &minor) == BadRequest) {
    VLOG(1) << "XInput2 not supported in the server.";
    xiopcode = -1;
    return;
  }
}

}  // namespace

namespace base {

MessagePumpX::MessagePumpX() : MessagePumpGlib(),
    x_source_(NULL),
    gdksource_(NULL),
    dispatching_event_(false),
    capture_x_events_(0),
    capture_gdk_events_(0) {
  if (use_gtk_message_pump) {
    gdk_window_add_filter(NULL, &GdkEventFilter, this);
    gdk_event_handler_set(&EventDispatcherX, this, NULL);
  } else {
    GPollFD* x_poll = new GPollFD();
    x_poll->fd = ConnectionNumber(g_xdisplay);
    x_poll->events = G_IO_IN;

    x_source_ = g_source_new(&XSourceFuncs, sizeof(GSource));
    g_source_add_poll(x_source_, x_poll);
    g_source_set_can_recurse(x_source_, FALSE);
    g_source_attach(x_source_, g_main_context_default());
  }

  InitializeXInput2();
  if (use_gtk_message_pump)
    InitializeEventsToCapture();
}

MessagePumpX::~MessagePumpX() {
  if (use_gtk_message_pump) {
    gdk_window_remove_filter(NULL, &GdkEventFilter, this);
    gdk_event_handler_set(reinterpret_cast<GdkEventFunc>(gtk_main_do_event),
        this, NULL);
  } else {
    g_source_destroy(x_source_);
    g_source_unref(x_source_);
    XCloseDisplay(g_xdisplay);
    g_xdisplay = NULL;
  }
}

// static
void MessagePumpX::DisableGtkMessagePump() {
  use_gtk_message_pump = false;
  g_xdisplay = XOpenDisplay(NULL);
}

// static
Display* MessagePumpX::GetDefaultXDisplay() {
  if (use_gtk_message_pump) {
    static GdkDisplay* display = gdk_display_get_default();
    return display ? GDK_DISPLAY_XDISPLAY(display) : NULL;
  } else {
    return g_xdisplay;
  }
}

// static
bool MessagePumpX::HasXInput2() {
  return xiopcode != -1;
}

bool MessagePumpX::ShouldCaptureXEvent(XEvent* xev) {
  return (!use_gtk_message_pump || capture_x_events_[xev->type])
      && (xev->type != GenericEvent || xev->xcookie.extension == xiopcode);
}

bool MessagePumpX::ProcessXEvent(XEvent* xev) {
  bool should_quit = false;

  bool have_cookie = false;
  if (xev->type == GenericEvent &&
      XGetEventData(xev->xgeneric.display, &xev->xcookie)) {
    have_cookie = true;
  }

  if (WillProcessXEvent(xev) == MessagePumpObserver::EVENT_CONTINUE) {
    MessagePumpDispatcher::DispatchStatus status =
        GetDispatcher()->Dispatch(xev);

    if (status == MessagePumpDispatcher::EVENT_QUIT) {
      should_quit = true;
      Quit();
    } else if (status == MessagePumpDispatcher::EVENT_IGNORED) {
      VLOG(1) << "Event (" << xev->type << ") not handled.";
    }
  }

  if (have_cookie) {
    XFreeEventData(xev->xgeneric.display, &xev->xcookie);
  }

  return should_quit;
}

bool MessagePumpX::RunOnce(GMainContext* context, bool block) {
  Display* display = GetDefaultXDisplay();
  if (!display || !GetDispatcher())
    return g_main_context_iteration(context, block);

  if (XPending(display)) {
    XEvent xev;
    XPeekEvent(display, &xev);

    if (ShouldCaptureXEvent(&xev)) {
      XNextEvent(display, &xev);
      if (ProcessXEvent(&xev))
        return true;
    } else if (use_gtk_message_pump && gdksource_) {
      // TODO(sad): A couple of extra events can still sneak in during this.
      // Those should be sent back to the X queue from the dispatcher
      // EventDispatcherX.
      gdksource_->source_funcs->dispatch = gdkdispatcher_;
      g_main_context_iteration(context, FALSE);
    }
  }

  bool retvalue;
  if (gdksource_ && use_gtk_message_pump) {
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

GdkFilterReturn MessagePumpX::GdkEventFilter(GdkXEvent* gxevent,
                                             GdkEvent* gevent,
                                             gpointer data) {
  MessagePumpX* pump = static_cast<MessagePumpX*>(data);
  XEvent* xev = static_cast<XEvent*>(gxevent);

  if (pump->ShouldCaptureXEvent(xev) && pump->GetDispatcher()) {
    pump->ProcessXEvent(xev);
    return GDK_FILTER_REMOVE;
  }
  CHECK(use_gtk_message_pump) << "GdkEvent:" << gevent->type;
  return GDK_FILTER_CONTINUE;
}

bool MessagePumpX::WillProcessXEvent(XEvent* xevent) {
  ObserverListBase<MessagePumpObserver>::Iterator it(observers());
  MessagePumpObserver* obs;
  while ((obs = it.GetNext()) != NULL) {
    if (obs->WillProcessXEvent(xevent))
      return true;
  }
  return false;
}

void MessagePumpX::EventDispatcherX(GdkEvent* event, gpointer data) {
  MessagePumpX* pump_x = reinterpret_cast<MessagePumpX*>(data);
  CHECK(use_gtk_message_pump) << "GdkEvent:" << event->type;

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

  gtk_main_do_event(event);
}

void MessagePumpX::InitializeEventsToCapture(void) {
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

  capture_x_events_[GenericEvent] = true;
}

MessagePumpObserver::EventStatus
    MessagePumpObserver::WillProcessXEvent(XEvent* xev) {
  return EVENT_CONTINUE;
}

COMPILE_ASSERT(XLASTEvent >= LASTEvent, XLASTEvent_too_small);

}  // namespace base
