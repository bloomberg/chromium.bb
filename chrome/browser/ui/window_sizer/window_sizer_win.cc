// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"

// How much horizontal and vertical offset there is between newly
// opened windows.
const int WindowSizer::kWindowTilePixels = 10;

// static
gfx::Point WindowSizer::GetDefaultPopupOrigin(const gfx::Size& size,
                                              chrome::HostDesktopType type) {
  RECT area;
  SystemParametersInfo(SPI_GETWORKAREA, 0, &area, 0);
  gfx::Point corner(area.left, area.top);

  if (Browser* browser = chrome::FindLastActiveWithHostDesktopType(type)) {
    RECT browser_rect;
    HWND window = reinterpret_cast<HWND>(browser->window()->GetNativeWindow());
    if (GetWindowRect(window, &browser_rect)) {
      // Limit to not overflow the work area right and bottom edges.
      gfx::Point limit(
          std::min(browser_rect.left + kWindowTilePixels,
                   area.right-size.width()),
          std::min(browser_rect.top + kWindowTilePixels,
                   area.bottom-size.height())
      );
      // Adjust corner to now overflow the work area left and top edges, so
      // that if a popup does not fit the title-bar is remains visible.
      corner = gfx::Point(
          std::max(corner.x(), limit.x()),
          std::max(corner.y(), limit.y())
      );
    }
  }
  return corner;
}
