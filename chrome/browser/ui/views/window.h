// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WINDOW_H_
#define CHROME_BROWSER_UI_VIEWS_WINDOW_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

namespace views {
class Widget;
class WidgetDelegate;
}

namespace browser {

// Create a window for given |delegate| using default frame view.
views::Widget* CreateViewsWindow(gfx::NativeWindow parent,
                                 views::WidgetDelegate* delegate);

}  // namespace browser

#endif  // CHROME_BROWSER_UI_VIEWS_WINDOW_H_
