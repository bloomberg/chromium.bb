// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_x.h"

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

// The opcode used for checking events.
int xiopcode = -1;

// The message-pump opens a connection to the display and owns it.
Display* g_xdisplay = NULL;

// The default dispatcher to process native events when no dispatcher
// is specified.
base::MessagePumpDispatcher* g_default_dispatcher = NULL;

void InitializeXInput2(void) {
  Display* display = base::MessagePumpX::GetDefaultXDisplay();
  if (!display)
    return;

  int event, err;

  if (!XQueryExtension(display, "XInputExtension", &xiopcode, &event, &err)) {
    DVLOG(1) << "X Input extension not available.";
    xiopcode = -1;
    return;
  }

#if defined(USE_XI2_MT)
  // USE_XI2_MT also defines the required XI2 minor minimum version.
  int major = 2, minor = USE_XI2_MT;
#else
  int major = 2, minor = 0;
#endif
  if (XIQueryVersion(display, &major, &minor) == BadRequest) {
    DVLOG(1) << "XInput2 not supported in the server.";
    xiopcode = -1;
    return;
  }
#if defined(USE_XI2_MT)
  if (major < 2 || (major == 2 && minor < USE_XI2_MT)) {
    DVLOG(1) << "XI version on server is " << major << "." << minor << ". "
            << "But 2." << USE_XI2_MT << " is required.";
    xiopcode = -1;
    return;
  }
#endif
}

}  // namespace

namespace base {

MessagePumpX::MessagePumpX() : MessagePumpGlib(),
    x_source_(NULL) {
  InitializeXInput2();
  InitXSource();
}

MessagePumpX::~MessagePumpX() {
  g_source_destroy(x_source_);
  g_source_unref(x_source_);
  XCloseDisplay(g_xdisplay);
  g_xdisplay = NULL;
}

// static
Display* MessagePumpX::GetDefaultXDisplay() {
  if (!g_xdisplay)
    g_xdisplay = XOpenDisplay(NULL);
  return g_xdisplay;
}

// static
bool MessagePumpX::HasXInput2() {
  return xiopcode != -1;
}

// static
void MessagePumpX::SetDefaultDispatcher(MessagePumpDispatcher* dispatcher) {
  DCHECK(!g_default_dispatcher || !dispatcher);
  g_default_dispatcher = dispatcher;
}

void MessagePumpX::InitXSource() {
  DCHECK(!x_source_);
  GPollFD* x_poll = new GPollFD();
  Display* display = GetDefaultXDisplay();
  DCHECK(display) << "Unable to get connection to X server";
  x_poll->fd = ConnectionNumber(display);
  x_poll->events = G_IO_IN;

  x_source_ = g_source_new(&XSourceFuncs, sizeof(GSource));
  g_source_add_poll(x_source_, x_poll);
  g_source_set_can_recurse(x_source_, FALSE);
  g_source_attach(x_source_, g_main_context_default());
}

bool MessagePumpX::ProcessXEvent(MessagePumpDispatcher* dispatcher,
                                 XEvent* xev) {
  bool should_quit = false;

  bool have_cookie = false;
  if (xev->type == GenericEvent &&
      XGetEventData(xev->xgeneric.display, &xev->xcookie)) {
    have_cookie = true;
  }

  if (WillProcessXEvent(xev) == EVENT_CONTINUE) {
    MessagePumpDispatcher::DispatchStatus status =
        dispatcher->Dispatch(xev);

    if (status == MessagePumpDispatcher::EVENT_QUIT) {
      should_quit = true;
      Quit();
    } else if (status == MessagePumpDispatcher::EVENT_IGNORED) {
      DVLOG(1) << "Event (" << xev->type << ") not handled.";
    }
    DidProcessXEvent(xev);
  }

  if (have_cookie) {
    XFreeEventData(xev->xgeneric.display, &xev->xcookie);
  }

  return should_quit;
}

bool MessagePumpX::RunOnce(GMainContext* context, bool block) {
  Display* display = GetDefaultXDisplay();
  MessagePumpDispatcher* dispatcher =
      GetDispatcher() ? GetDispatcher() : g_default_dispatcher;

  if (!display || !dispatcher)
    return g_main_context_iteration(context, block);

  // In the general case, we want to handle all pending events before running
  // the tasks. This is what happens in the message_pump_glib case.
  while (XPending(display)) {
    XEvent xev;
    XNextEvent(display, &xev);
    if (ProcessXEvent(dispatcher, &xev))
      return true;
  }

  return g_main_context_iteration(context, block);
}

bool MessagePumpX::WillProcessXEvent(XEvent* xevent) {
  ObserverListBase<MessagePumpObserver>::Iterator it(observers());
  MessagePumpObserver* obs;
  while ((obs = it.GetNext()) != NULL) {
    if (obs->WillProcessEvent(xevent))
      return true;
  }
  return false;
}

void MessagePumpX::DidProcessXEvent(XEvent* xevent) {
  ObserverListBase<MessagePumpObserver>::Iterator it(observers());
  MessagePumpObserver* obs;
  while ((obs = it.GetNext()) != NULL) {
    obs->DidProcessEvent(xevent);
  }
}

}  // namespace base
