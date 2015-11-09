// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_IMPL_H_

#include <set>

#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

gfx::NativeWindow GetLocalProcessWindowAtPointImpl(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore,
    const std::set<int>& ignore_ids,
    gfx::NativeWindow window);

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_IMPL_H_
