// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "ui/gfx/screen.h"

// Used to pad the default new window size.  On Windows, this is also used for
// positioning new windows (each window is offset from the previous one).
// Since we don't position windows, it's only used for the default new window
// size.
const int WindowSizer::kWindowTilePixels = 10;

// static
gfx::Point WindowSizer::GetDefaultPopupOrigin(const gfx::Size& size,
                                              chrome::HostDesktopType type) {
  gfx::Rect monitor_bounds = gfx::Screen::GetNativeScreen()->
      GetPrimaryDisplay().work_area();
  gfx::Point corner(monitor_bounds.x(), monitor_bounds.y());

  if (Browser* browser = chrome::FindLastActiveWithHostDesktopType(type)) {
    GtkWindow* window =
        reinterpret_cast<GtkWindow*>(browser->window()->GetNativeWindow());
    int x = 0, y = 0;
    gtk_window_get_position(window, &x, &y);
    // Limit to not overflow the work area right and bottom edges.
    gfx::Point limit(
        std::min(x + kWindowTilePixels, monitor_bounds.right() - size.width()),
        std::min(y + kWindowTilePixels,
                 monitor_bounds.bottom() - size.height()));
    // Adjust corner to now overflow the work area left and top edges, so
    // that if a popup does not fit the title-bar is remains visible.
    corner = gfx::Point(
        std::max(corner.x(), limit.x()),
        std::max(corner.y(), limit.y()));
  }
  return corner;
}
