// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_TABS_WINDOW_FINDER_H_
#define CHROME_BROWSER_UI_GTK_TABS_WINDOW_FINDER_H_

#include <set>

typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

namespace gfx {
class Point;
}

// Returns the Window at the specified point, ignoring the windows in |ignore|.
GtkWindow* GetLocalProcessWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<GtkWidget*>& ignore);

#endif  // CHROME_BROWSER_UI_GTK_TABS_WINDOW_FINDER_H_
