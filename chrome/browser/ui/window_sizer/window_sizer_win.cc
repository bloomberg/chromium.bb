// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"

// How much horizontal and vertical offset there is between newly
// opened windows.
const int WindowSizer::kWindowTilePixels = 10;

// static
gfx::Point WindowSizer::GetDefaultPopupOrigin(const gfx::Size& size) {
  RECT area;
  SystemParametersInfo(SPI_GETWORKAREA, 0, &area, 0);
  gfx::Point corner(area.left, area.top);

  if (Browser* b = BrowserList::GetLastActive()) {
    RECT browser;
    HWND window = reinterpret_cast<HWND>(b->window()->GetNativeWindow());
    if (GetWindowRect(window, &browser)) {
      // Limit to not overflow the work area right and bottom edges.
      gfx::Point limit(
          std::min(browser.left + kWindowTilePixels, area.right-size.width()),
          std::min(browser.top + kWindowTilePixels, area.bottom-size.height())
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
