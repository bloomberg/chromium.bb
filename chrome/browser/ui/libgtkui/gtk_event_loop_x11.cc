// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/gtk_event_loop_x11.h"

#include "ui/gfx/x/x11.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "base/memory/singleton.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11_types.h"

namespace libgtkui {

namespace {

// Xkb Events store group attribute into XKeyEvent::state bit field, along with
// other state-related info, while GdkEventKey objects have separate fields for
// that purpose, they are ::state and ::group. This function is responsible for
// recomposing them into a single bit field value when translating GdkEventKey
// into XKeyEvent. This is similar to XkbBuildCoreState(), but assumes state is
// an uint rather than an uchar.
//
// More details:
// https://gitlab.freedesktop.org/xorg/proto/xorgproto/blob/master/include/X11/extensions/XKB.h#L372
int BuildXkbStateFromGdkEvent(const GdkEventKey& keyev) {
  DCHECK_EQ(0u, XkbGroupForCoreState(keyev.state));
  DCHECK(XkbIsLegalGroup(keyev.group));
  return keyev.state | ((keyev.group & 0x3) << 13);
}

}  // namespace

// static
GtkEventLoopX11* GtkEventLoopX11::GetInstance() {
  return base::Singleton<GtkEventLoopX11>::get();
}

GtkEventLoopX11::GtkEventLoopX11() {
  gdk_event_handler_set(DispatchGdkEvent, nullptr, nullptr);
}

GtkEventLoopX11::~GtkEventLoopX11() {
  gdk_event_handler_set(reinterpret_cast<GdkEventFunc>(gtk_main_do_event),
                        nullptr, nullptr);
}

// static
void GtkEventLoopX11::DispatchGdkEvent(GdkEvent* gdk_event, gpointer) {
  switch (gdk_event->type) {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      ProcessGdkEventKey(gdk_event->key);
      break;
    default:
      break;  // Do nothing.
  }

  gtk_main_do_event(gdk_event);
}

// static
void GtkEventLoopX11::ProcessGdkEventKey(const GdkEventKey& gdk_event_key) {
  // This function translates GdkEventKeys into XKeyEvents and puts them to
  // the X event queue.
  //
  // base::MessagePumpX11 is using the X11 event queue and all key events should
  // be processed there.  However, there are cases(*1) that GdkEventKeys are
  // created instead of XKeyEvents.  In these cases, we have to translate
  // GdkEventKeys to XKeyEvents and puts them to the X event queue so our main
  // event loop can handle those key events.
  //
  // (*1) At least ibus-gtk in async mode creates a copy of user's key event and
  // pushes it back to the GDK event queue.  In this case, there is no
  // corresponding key event in the X event queue.  So we have to handle this
  // case.  ibus-gtk is used through gtk-immodule to support IMEs.

  XEvent x_event;
  x_event.xkey = {};
  x_event.xkey.type =
      gdk_event_key.type == GDK_KEY_PRESS ? KeyPress : KeyRelease;
  x_event.xkey.send_event = gdk_event_key.send_event;
  x_event.xkey.display = gfx::GetXDisplay();
  x_event.xkey.window = GDK_WINDOW_XID(gdk_event_key.window);
  x_event.xkey.root = DefaultRootWindow(x_event.xkey.display);
  x_event.xkey.time = gdk_event_key.time;
  x_event.xkey.state = BuildXkbStateFromGdkEvent(gdk_event_key);
  x_event.xkey.keycode = gdk_event_key.hardware_keycode;
  x_event.xkey.same_screen = true;

  // We want to process the gtk event; mapped to an X11 event immediately
  // otherwise if we put it back on the queue we may get items out of order.
  if (ui::X11EventSource* x11_source = ui::X11EventSource::GetInstance())
    x11_source->DispatchXEventNow(&x_event);
  else
    XPutBackEvent(x_event.xkey.display, &x_event);
}

}  // namespace libgtkui
