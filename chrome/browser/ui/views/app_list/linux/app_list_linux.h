// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_LINUX_APP_LIST_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_LINUX_APP_LIST_LINUX_H_

#include "chrome/browser/ui/app_list/app_list_positioner.h"

namespace app_list {
class AppListView;
}

namespace gfx {
class Display;
class Point;
class Size;
}  // namespace gfx

// Responsible for positioning an AppListView on Linux.
// TODO(tapted): Shouldn't be a class - move the static member functions out.
class AppListLinux {
 public:
  // Determines which screen edge the shelf is aligned to. This tries to find
  // the edge of the surface where the user normally launches apps from (so, for
  // example, on Gnome Classic, this is the applications menu, not the taskbar).
  static AppListPositioner::ScreenEdge ShelfLocationInDisplay(
      const gfx::Display& display);

  // Finds the position for a window to anchor it to the shelf. This chooses the
  // most appropriate position for the window based on whether the shelf exists,
  // the position of the shelf, and the mouse cursor. Returns the intended
  // coordinates for the center of the window. If |shelf_rect| is empty, assumes
  // there is no shelf on the given display.
  static gfx::Point FindAnchorPoint(const gfx::Size& view_size,
                                    const gfx::Display& display,
                                    const gfx::Point& cursor,
                                    AppListPositioner::ScreenEdge edge,
                                    bool center_window);

  static void MoveNearCursor(app_list::AppListView* view);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_LINUX_APP_LIST_LINUX_H_
