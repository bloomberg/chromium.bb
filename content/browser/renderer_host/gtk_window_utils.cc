// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/gtk_window_utils.h"

#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <vector>

#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gtk_compat.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/x/x11_types.h"

namespace content {

namespace {

// Returns the region of |window|'s desktop that isn't occupied by docks or
// panels.  Returns an empty rect on failure.
gfx::Rect GetWorkArea(Window window) {
  Window root = ui::GetX11RootWindow();
  int desktop = -1;
  if (!ui::GetIntProperty(window, "_NET_WM_DESKTOP", &desktop)) {
    // Hack for ion3: _NET_WM_DESKTOP doesn't get set on individual windows, but
    // _NET_CURRENT_DESKTOP and _NET_WORKAREA do get set.
    ui::GetIntProperty(root, "_NET_CURRENT_DESKTOP", &desktop);
  }
  if (desktop < 0)
    return gfx::Rect();

  std::vector<int> property;
  if (!ui::GetIntArrayProperty(root, "_NET_WORKAREA", &property))
    return gfx::Rect();

  size_t start_index = 4 * desktop;
  if (property.size() < start_index + 4)
    return gfx::Rect();

  return gfx::Rect(property[start_index], property[start_index + 1],
                   property[start_index + 2], property[start_index + 3]);
}

blink::WebScreenInfo GetScreenInfo(XDisplay* display, int screenNumber) {
  // XDisplayWidth() and XDisplayHeight() return cached values. To ensure that
  // we return the correct dimensions after the screen is resized, query the
  // root window's geometry each time.
  Window root = RootWindow(display, screenNumber);
  Window root_ret;
  int x, y;
  unsigned int width, height, border, depth;
  XGetGeometry(
      display, root, &root_ret, &x, &y, &width, &height, &border, &depth);

  blink::WebScreenInfo results;
  results.depthPerComponent = 8;  // Assume 8bpp, which is usually right.
  results.depth = depth;
  results.isMonochrome = depth == 1;
  results.rect = blink::WebRect(x, y, width, height);
  results.availableRect = results.rect;
  return results;
}

}  // namespace

void GetScreenInfoFromNativeWindow(
    GdkWindow* gdk_window, blink::WebScreenInfo* results) {
  GdkScreen* screen = gdk_window_get_screen(gdk_window);
  *results = GetScreenInfo(gdk_x11_drawable_get_xdisplay(gdk_window),
                        gdk_x11_screen_get_screen_number(screen));

  int monitor_number = gdk_screen_get_monitor_at_window(screen, gdk_window);
  GdkRectangle monitor_rect;
  gdk_screen_get_monitor_geometry(screen, monitor_number, &monitor_rect);
  results->rect = blink::WebRect(monitor_rect.x, monitor_rect.y,
                                  monitor_rect.width, monitor_rect.height);

  gfx::Rect available_rect = results->rect;
  gfx::Rect work_area = GetWorkArea(GDK_WINDOW_XID(gdk_window));
  if (!work_area.IsEmpty())
    available_rect.Intersect(work_area);
  results->availableRect = available_rect;
}

}  // namespace content
