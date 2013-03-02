// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_

#include "ui/gfx/native_widget_types.h"

namespace content {
class WebContents;
class BrowserContext;
}
namespace views {
class NonClientFrameView;
class WidgetDelegate;
class Widget;
}

views::Widget* CreateWebContentsModalDialogViews(
    views::WidgetDelegate* widget_delegate,
    gfx::NativeView parent);

views::NonClientFrameView* CreateConstrainedStyleNonClientFrameView(
    views::Widget* widget,
    content::BrowserContext* browser_context);

#endif  // CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
