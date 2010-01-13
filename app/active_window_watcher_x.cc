// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <X11/Xlib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "app/active_window_watcher_x.h"

static Atom kNetActiveWindowAtom = None;

// static
void ActiveWindowWatcherX::AddObserver(Observer* observer) {
  Singleton<ActiveWindowWatcherX>::get()->observers_.AddObserver(observer);
}

// static
void ActiveWindowWatcherX::RemoveObserver(Observer* observer) {
  Singleton<ActiveWindowWatcherX>::get()->observers_.RemoveObserver(observer);
}

ActiveWindowWatcherX::ActiveWindowWatcherX() {
  Init();
}

void ActiveWindowWatcherX::Init() {
  GdkAtom kNetActiveWindow = gdk_atom_intern("_NET_ACTIVE_WINDOW", FALSE);
  kNetActiveWindowAtom = gdk_x11_atom_to_xatom_for_display(
      gdk_screen_get_display(gdk_screen_get_default()), kNetActiveWindow);

  GdkWindow* root = gdk_get_default_root_window();
  // Set up X Event filter to listen for PropertyChange X events.  These events
  // tell us when the active window changes.
  gdk_window_add_filter(root, &ActiveWindowWatcherX::OnWindowXEvent, this);
  XSelectInput(
      GDK_WINDOW_XDISPLAY(root), GDK_WINDOW_XID(root), PropertyChangeMask);
}

void ActiveWindowWatcherX::NotifyActiveWindowChanged() {
  // We don't use gdk_screen_get_active_window() because it caches
  // whether or not the window manager supports _NET_ACTIVE_WINDOW.
  // This causes problems at startup for chromiumos.
  Atom type = None;
  int format = 0;  // size in bits of each item in 'property'
  long unsigned int num_items = 0, remaining_bytes = 0;
  unsigned char* property = NULL;

  XGetWindowProperty(gdk_x11_get_default_xdisplay(),
                     GDK_WINDOW_XID(gdk_get_default_root_window()),
                     kNetActiveWindowAtom,
                     0,      // offset into property data to read
                     1,      // length to get in 32-bit quantities
                     False,  // deleted
                     AnyPropertyType,
                     &type,
                     &format,
                     &num_items,
                     &remaining_bytes,
                     &property);

  // Check that the property was set and contained a single 32-bit item (we
  // don't check that remaining_bytes is 0, though, as XFCE's window manager
  // seems to actually store two values in the property for some unknown
  // reason.)
  if (format == 32 && num_items == 1) {
    int xid = *reinterpret_cast<int*>(property);
    GdkWindow* active_window = gdk_window_lookup(xid);
    FOR_EACH_OBSERVER(
        Observer,
        observers_,
        ActiveWindowChanged(active_window));
  }
  if (property)
    XFree(property);
}

GdkFilterReturn ActiveWindowWatcherX::OnWindowXEvent(GdkXEvent* xevent,
    GdkEvent* event, gpointer window_watcher) {
  ActiveWindowWatcherX* watcher = reinterpret_cast<ActiveWindowWatcherX*>(
      window_watcher);
  XEvent* xev = static_cast<XEvent*>(xevent);

  if (xev->xany.type == PropertyNotify &&
      xev->xproperty.atom == kNetActiveWindowAtom) {
    watcher->NotifyActiveWindowChanged();
  }

  return GDK_FILTER_CONTINUE;
}
