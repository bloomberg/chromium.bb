// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/gtk_window_utils.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "ui/base/gtk/gtk_compat.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/x11/WebScreenInfoFactory.h"

namespace content {

void GetScreenInfoFromNativeWindow(
    GdkWindow* gdk_window, WebKit::WebScreenInfo* results) {
  GdkScreen* screen = gdk_window_get_screen(gdk_window);
  *results = WebKit::WebScreenInfoFactory::screenInfo(
      gdk_x11_drawable_get_xdisplay(gdk_window),
      gdk_x11_screen_get_screen_number(screen));

  // TODO(tony): We should move this code into WebScreenInfoFactory.
  int monitor_number = gdk_screen_get_monitor_at_window(screen, gdk_window);
  GdkRectangle monitor_rect;
  gdk_screen_get_monitor_geometry(screen, monitor_number, &monitor_rect);
  results->rect = WebKit::WebRect(monitor_rect.x, monitor_rect.y,
                                  monitor_rect.width, monitor_rect.height);
  // TODO(tony): Should we query _NET_WORKAREA to get the workarea?
  results->availableRect = results->rect;
}

}  // namespace content
