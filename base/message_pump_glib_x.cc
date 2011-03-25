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

#if defined(HAVE_XINPUT2)

// Setup XInput2 select for the GtkWidget.
gboolean GtkWidgetRealizeCallback(GSignalInvocationHint* hint, guint nparams,
                                  const GValue* pvalues, gpointer data) {
  GtkWidget* widget = GTK_WIDGET(g_value_get_object(pvalues));
  GdkWindow* window = widget->window;
  base::MessagePumpGlibX* msgpump = static_cast<base::MessagePumpGlibX*>(data);

  DCHECK(window);  // TODO(sad): Remove once determined if necessary.

  if (GDK_WINDOW_TYPE(window) != GDK_WINDOW_TOPLEVEL &&
      GDK_WINDOW_TYPE(window) != GDK_WINDOW_CHILD &&
      GDK_WINDOW_TYPE(window) != GDK_WINDOW_DIALOG)
    return true;

  // TODO(sad): Do we need to set a flag on |window| to make sure we don't
  // select for the same GdkWindow multiple times? Does it matter?
  msgpump->SetupXInput2ForXWindow(GDK_WINDOW_XID(window));

  return true;
}

// We need to capture all the GDK windows that get created, and start
// listening for XInput2 events. So we setup a callback to the 'realize'
// signal for GTK+ widgets, so that whenever the signal triggers for any
// GtkWidget, which means the GtkWidget should now have a GdkWindow, we can
// setup XInput2 events for the GdkWindow.
static guint realize_signal_id = 0;
static guint realize_hook_id = 0;

void SetupGtkWidgetRealizeNotifier(base::MessagePumpGlibX* msgpump) {
  gpointer klass = g_type_class_ref(GTK_TYPE_WIDGET);

  g_signal_parse_name("realize", GTK_TYPE_WIDGET,
                      &realize_signal_id, NULL, FALSE);
  realize_hook_id = g_signal_add_emission_hook(realize_signal_id, 0,
      GtkWidgetRealizeCallback, static_cast<gpointer>(msgpump), NULL);

  g_type_class_unref(klass);
}

void RemoveGtkWidgetRealizeNotifier() {
  if (realize_signal_id != 0)
    g_signal_remove_emission_hook(realize_signal_id, realize_hook_id);
  realize_signal_id = 0;
  realize_hook_id = 0;
}

#endif  // HAVE_XINPUT2

}  // namespace

namespace base {

MessagePumpGlibX::MessagePumpGlibX() : base::MessagePumpForUI(),
#if defined(HAVE_XINPUT2)
    xiopcode_(-1),
    pointer_devices_(),
#endif
    gdksource_(NULL),
    dispatching_event_(false),
    capture_x_events_(0),
    capture_gdk_events_(0) {
  gdk_event_handler_set(&EventDispatcherX, this, NULL);

#if defined(HAVE_XINPUT2)
  InitializeXInput2();
#endif
  InitializeEventsToCapture();
}

MessagePumpGlibX::~MessagePumpGlibX() {
#if defined(HAVE_XINPUT2)
  RemoveGtkWidgetRealizeNotifier();
#endif
}

#if defined(HAVE_XINPUT2)
void MessagePumpGlibX::SetupXInput2ForXWindow(Window xwindow) {
  Display* xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());

  // Setup mask for mouse events.
  unsigned char mask[(XI_LASTEVENT + 7)/8];
  memset(mask, 0, sizeof(mask));

  XISetMask(mask, XI_ButtonPress);
  XISetMask(mask, XI_ButtonRelease);
  XISetMask(mask, XI_Motion);

  XIEventMask evmasks[pointer_devices_.size()];
  int count = 0;
  for (std::set<int>::const_iterator iter = pointer_devices_.begin();
       iter != pointer_devices_.end();
       ++iter, ++count) {
    evmasks[count].deviceid = *iter;
    evmasks[count].mask_len = sizeof(mask);
    evmasks[count].mask = mask;
  }

  XISelectEvents(xdisplay, xwindow, evmasks, pointer_devices_.size());

  // TODO(sad): Setup masks for keyboard events.

  XFlush(xdisplay);
}
#endif  // HAVE_XINPUT2

bool MessagePumpGlibX::RunOnce(GMainContext* context, bool block) {
  GdkDisplay* gdisp = gdk_display_get_default();
  if (!gdisp || !GetDispatcher())
    return MessagePumpForUI::RunOnce(context, block);

  Display* display = GDK_DISPLAY_XDISPLAY(gdisp);
  bool should_quit = false;

  if (XPending(display)) {
    XEvent xev;
    XPeekEvent(display, &xev);
    if (capture_x_events_[xev.type]
#if defined(HAVE_XINPUT2)
        && (xev.type != GenericEvent || xev.xcookie.extension == xiopcode_)
#endif
        ) {
      XNextEvent(display, &xev);

#if defined(HAVE_XINPUT2)
      bool have_cookie = false;
      if (xev.type == GenericEvent &&
          XGetEventData(xev.xgeneric.display, &xev.xcookie)) {
        have_cookie = true;
      }
#endif

      MessagePumpGlibXDispatcher::DispatchStatus status =
          static_cast<MessagePumpGlibXDispatcher*>
          (GetDispatcher())->DispatchX(&xev);

      if (status == MessagePumpGlibXDispatcher::EVENT_QUIT) {
        should_quit = true;
        Quit();
      } else if (status == MessagePumpGlibXDispatcher::EVENT_IGNORED) {
        DLOG(WARNING) << "Event (" << xev.type << ") not handled.";

        // TODO(sad): It is necessary to put back the event so that the default
        // GDK events handler can take care of it. Without this, it is
        // impossible to use the omnibox at the moment. However, this will
        // eventually be removed once the omnibox code is updated for touchui.
        XPutBackEvent(display, &xev);
        if (gdksource_)
          gdksource_->source_funcs->dispatch = gdkdispatcher_;
        g_main_context_iteration(context, FALSE);
      }

#if defined(HAVE_XINPUT2)
      if (have_cookie) {
        XFreeEventData(xev.xgeneric.display, &xev.xcookie);
      }
#endif
    } else {
      // TODO(sad): A couple of extra events can still sneak in during this.
      // Those should be sent back to the X queue from the dispatcher
      // EventDispatcherX.
      if (gdksource_)
        gdksource_->source_funcs->dispatch = gdkdispatcher_;
      g_main_context_iteration(context, FALSE);
    }
  }

  if (should_quit)
    return true;

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

void MessagePumpGlibX::EventDispatcherX(GdkEvent* event, gpointer data) {
  MessagePumpGlibX* pump_x = reinterpret_cast<MessagePumpGlibX*>(data);

  if (!pump_x->gdksource_) {
    pump_x->gdksource_ = g_main_current_source();
    if (pump_x->gdksource_)
      pump_x->gdkdispatcher_ = pump_x->gdksource_->source_funcs->dispatch;
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

  // TODO(sad): Here, we only setup so that the X windows created by GTK+ are
  // setup for XInput2 events. We need a way to listen for XInput2 events for X
  // windows created by other means (e.g. for context menus).
  SetupGtkWidgetRealizeNotifier(this);

  // Instead of asking X for the list of devices all the time, let's maintain a
  // list of pointer devices we care about.
  // It is not necessary to select for slave devices. XInput2 provides enough
  // information to the event callback to decide which slave device triggered
  // the event, thus decide whether the 'pointer event' is a 'mouse event' or a
  // 'touch event'.
  // If the touch device has 'GrabDevice' set and 'SendCoreEvents' unset (which
  // is possible), then the device is detected as a floating device, and a
  // floating device is not connected to a master device. So it is necessary to
  // also select on the floating devices.
  int count = 0;
  XIDeviceInfo* devices = XIQueryDevice(xdisplay, XIAllDevices, &count);
  for (int i = 0; i < count; i++) {
    XIDeviceInfo* devinfo = devices + i;
    if (devinfo->use == XIFloatingSlave || devinfo->use == XIMasterPointer)
      pointer_devices_.insert(devinfo->deviceid);
  }
  XIFreeDeviceInfo(devices);

  // TODO(sad): Select on root for XI_HierarchyChanged so that floats_ and
  // masters_ can be kept up-to-date. This is a relatively rare event, so we can
  // put it off for a later time.
  // Note: It is not necessary to listen for XI_DeviceChanged events.
}
#endif  // HAVE_XINPUT2

}  // namespace base
