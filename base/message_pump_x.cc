// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
                         gpointer data) {
  base::MessagePumpX* pump = static_cast<base::MessagePumpX*>(data);
  return pump->DispatchXEvents();
}

GSourceFuncs XSourceFuncs = {
  XSourcePrepare,
  XSourceCheck,
  XSourceDispatch,
  NULL
};

// The message-pump opens a connection to the display and owns it.
Display* g_xdisplay = NULL;

// The default dispatcher to process native events when no dispatcher
// is specified.
base::MessagePumpDispatcher* g_default_dispatcher = NULL;

bool InitializeXInput2Internal() {
  Display* display = base::MessagePumpX::GetDefaultXDisplay();
  if (!display)
    return false;

  int event, err;

  int xiopcode;
  if (!XQueryExtension(display, "XInputExtension", &xiopcode, &event, &err)) {
    DVLOG(1) << "X Input extension not available.";
    return false;
  }

#if defined(USE_XI2_MT)
  // USE_XI2_MT also defines the required XI2 minor minimum version.
  int major = 2, minor = USE_XI2_MT;
#else
  int major = 2, minor = 0;
#endif
  if (XIQueryVersion(display, &major, &minor) == BadRequest) {
    DVLOG(1) << "XInput2 not supported in the server.";
    return false;
  }
#if defined(USE_XI2_MT)
  if (major < 2 || (major == 2 && minor < USE_XI2_MT)) {
    DVLOG(1) << "XI version on server is " << major << "." << minor << ". "
            << "But 2." << USE_XI2_MT << " is required.";
    return false;
  }
#endif

  return true;
}

bool InitializeXInput2() {
  static bool xinput2_supported = InitializeXInput2Internal();
  return xinput2_supported;
}

}  // namespace

namespace base {

MessagePumpX::MessagePumpX() : MessagePumpGlib(),
    x_source_(NULL) {
  InitializeXInput2();
  InitXSource();
}

// static
Display* MessagePumpX::GetDefaultXDisplay() {
  if (!g_xdisplay)
    g_xdisplay = XOpenDisplay(NULL);
  return g_xdisplay;
}

// static
bool MessagePumpX::HasXInput2() {
  return InitializeXInput2();
}

// static
void MessagePumpX::SetDefaultDispatcher(MessagePumpDispatcher* dispatcher) {
  DCHECK(!g_default_dispatcher || !dispatcher);
  g_default_dispatcher = dispatcher;
}

gboolean MessagePumpX::DispatchXEvents() {
  Display* display = GetDefaultXDisplay();
  DCHECK(display);
  MessagePumpDispatcher* dispatcher =
      GetDispatcher() ? GetDispatcher() : g_default_dispatcher;

  // In the general case, we want to handle all pending events before running
  // the tasks. This is what happens in the message_pump_glib case.
  while (XPending(display)) {
    XEvent xev;
    XNextEvent(display, &xev);
    if (dispatcher && ProcessXEvent(dispatcher, &xev))
      return TRUE;
  }
  return TRUE;
}

MessagePumpX::~MessagePumpX() {
  g_source_destroy(x_source_);
  g_source_unref(x_source_);
  XCloseDisplay(g_xdisplay);
  g_xdisplay = NULL;
}

void MessagePumpX::InitXSource() {
  // CHECKs are to help track down crbug.com/113106.
  CHECK(!x_source_);
  Display* display = GetDefaultXDisplay();
  CHECK(display) << "Unable to get connection to X server";
  x_poll_.reset(new GPollFD());
  CHECK(x_poll_.get());
  x_poll_->fd = ConnectionNumber(display);
  x_poll_->events = G_IO_IN;

  x_source_ = g_source_new(&XSourceFuncs, sizeof(GSource));
  g_source_add_poll(x_source_, x_poll_.get());
  g_source_set_can_recurse(x_source_, TRUE);
  g_source_set_callback(x_source_, NULL, this, NULL);
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

  if (!WillProcessXEvent(xev)) {
    if (!dispatcher->Dispatch(xev)) {
      should_quit = true;
      Quit();
    }
    DidProcessXEvent(xev);
  }

  if (have_cookie) {
    XFreeEventData(xev->xgeneric.display, &xev->xcookie);
  }

  return should_quit;
}

bool MessagePumpX::WillProcessXEvent(XEvent* xevent) {
  if (!observers().might_have_observers())
    return false;
  ObserverListBase<MessagePumpObserver>::Iterator it(observers());
  MessagePumpObserver* obs;
  while ((obs = it.GetNext()) != NULL) {
    if (obs->WillProcessEvent(xevent))
      return true;
  }
  return false;
}

void MessagePumpX::DidProcessXEvent(XEvent* xevent) {
  FOR_EACH_OBSERVER(MessagePumpObserver, observers(), DidProcessEvent(xevent));
}

}  // namespace base
