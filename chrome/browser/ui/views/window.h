// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WINDOW_H_
#define CHROME_BROWSER_UI_VIEWS_WINDOW_H_
#pragma once

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

namespace views {
class BubbleDelegateView;
class Widget;
class WidgetDelegate;
}

namespace browser {

// Create a frameless window for given |delegate|.
views::Widget* CreateFramelessViewsWindow(gfx::NativeWindow parent,
                                          views::WidgetDelegate* delegate);

// Create a framelss window with given |delegate|, |parent| and |bounds|.
views::Widget* CreateFramelessWindowWithParentAndBounds(
    views::WidgetDelegate* delegate,
    gfx::NativeWindow parent,
    const gfx::Rect& bounds);

// Create a bubble that will show even when chromeos screen is locked,
// for a given |delegate|. Takes care of chromeos specific window
// type setting. For bubbles that do not need to be shown when screen
// is locked, use CreateViewsBubble instead.
views::Widget* CreateViewsBubbleAboveLockScreen(
    views::BubbleDelegateView* delegate);

}  // namespace browser

#endif  // CHROME_BROWSER_UI_VIEWS_WINDOW_H_
