// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_H_

#include <set>

#include "chrome/browser/ui/host_desktop.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

// Returns the Window at the specified point, ignoring the windows in |ignore|.
aura::Window* GetLocalProcessWindowAtPoint(
    chrome::HostDesktopType host_desktop_type,
    const gfx::Point& screen_point,
    const std::set<aura::Window*>& ignore);

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_H_
