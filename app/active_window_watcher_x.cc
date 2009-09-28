// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <X11/Xlib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "app/active_window_watcher_x.h"

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
  // Set up X Event filter to listen for PropertyChange X events.  These events
  // tell us when the active window changes.
  GdkWindow* root = gdk_screen_get_root_window(gdk_screen_get_default());
  gdk_window_add_filter(root, &ActiveWindowWatcherX::OnWindowXEvent, this);
  XSelectInput(GDK_WINDOW_XDISPLAY(root), GDK_WINDOW_XID(root),
               PropertyChangeMask);
}

void ActiveWindowWatcherX::NotifyActiveWindowChanged() {
  GdkWindow* active_window = gdk_screen_get_active_window(
      gdk_screen_get_default());

  // If the window manager doesn't support _NET_ACTIVE_WINDOW, we don't know
  // which window is active and just give up.
  if (!active_window)
    return;

  FOR_EACH_OBSERVER(Observer, observers_, ActiveWindowChanged(active_window));
}

GdkFilterReturn ActiveWindowWatcherX::OnWindowXEvent(GdkXEvent* xevent,
    GdkEvent* event, gpointer window_watcher) {
  static const GdkAtom kNetActiveWindow = gdk_atom_intern(
      "_NET_ACTIVE_WINDOW", FALSE);
  static const Atom kNetActiveWindowAtom = gdk_x11_atom_to_xatom_for_display(
        gdk_screen_get_display(gdk_screen_get_default()), kNetActiveWindow);

  ActiveWindowWatcherX* watcher = reinterpret_cast<ActiveWindowWatcherX*>(
      window_watcher);
  XEvent* xev = static_cast<XEvent*>(xevent);

  if (xev->xany.type == PropertyNotify &&
      xev->xproperty.atom == kNetActiveWindowAtom) {
    watcher->NotifyActiveWindowChanged();
  }

  return GDK_FILTER_CONTINUE;
}
