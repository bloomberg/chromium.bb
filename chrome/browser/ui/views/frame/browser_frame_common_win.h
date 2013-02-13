// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_COMMON_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_COMMON_WIN_H_

class BrowserView;
class NativeWidget;

namespace ui {
class ThemeProvider;
}

namespace views {
namespace internal {
class NativeWidgetPrivate;
}
}

namespace chrome {
// Returns true if we should use the native i.e. Glass browser frame.
// for the BrowserView passed in.
bool ShouldUseNativeFrame(
    const views::internal::NativeWidgetPrivate* native_widget,
    const BrowserView* browser_view,
    const ui::ThemeProvider* theme_provider);
}

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_COMMON_WIN_H_
