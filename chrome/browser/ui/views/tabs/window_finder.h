// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_H_

#include <set>

#include "chrome/browser/ui/host_desktop.h"

namespace gfx {
class Point;
}

// Returns the Window at the specified point, ignoring the windows in |ignore|.
// On Android, |source| is required to access root window. All other platforms
// do not use the parameter.
// TODO(bshe): Remove |source| once we have a way to get root window from
// |screen_point| on Android. See crbug.com/549735
gfx::NativeWindow GetLocalProcessWindowAtPoint(
    chrome::HostDesktopType host_desktop_type,
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore,
    gfx::NativeWindow source);

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_H_
