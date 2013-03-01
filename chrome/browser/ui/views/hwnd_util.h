// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HWND_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_HWND_UTIL_H_

#include "ui/gfx/native_widget_types.h"

namespace views {
class Widget;
}

namespace chrome {

// Returns the HWND for the specified Widget.
HWND HWNDForWidget(views::Widget* widget);

// Returns the HWND for the specified NativeWindow.
HWND HWNDForNativeWindow(gfx::NativeWindow window);

}

#endif  // CHROME_BROWSER_UI_VIEWS_HWND_UTIL_H_
