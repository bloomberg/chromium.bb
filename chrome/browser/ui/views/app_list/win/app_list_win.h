// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_WIN_H_

namespace app_list {
class AppListView;
}

namespace gfx {
class Display;
class Point;
class Rect;
class Size;
}

// Responsible for positioning an AppListView on Windows.
// TODO(tapted): Shouldn't be a class - move the static member functions out.
class AppListWin {
 public:
  // Finds the position for a window to anchor it to the taskbar. This chooses
  // the most appropriate position for the window based on whether the taskbar
  // exists, the position of the taskbar, and the mouse cursor. Returns the
  // intended coordinates for the center of the window. If |taskbar_rect| is
  // empty, assumes there is no taskbar on the given display.
  static gfx::Point FindAnchorPoint(const gfx::Size& view_size,
                                    const gfx::Display& display,
                                    const gfx::Point& cursor,
                                    const gfx::Rect& taskbar_rect,
                                    bool center_window);

  static void MoveNearCursor(app_list::AppListView* view);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_WIN_H_
