// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WINDOW_H_
#define CHROME_BROWSER_UI_VIEWS_WINDOW_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
}

namespace views {
class Window;
class WindowDelegate;
}

namespace browser {

// Create a window for given |delegate| using default frame view.
views::Window* CreateViewsWindow(gfx::NativeWindow parent,
                                 const gfx::Rect& bounds,
                                 views::WindowDelegate* delegate);

}  // namespace browser

#endif  // CHROME_BROWSER_UI_VIEWS_WINDOW_H_
