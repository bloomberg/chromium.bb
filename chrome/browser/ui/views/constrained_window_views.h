// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_

#include "ui/gfx/native_widget_types.h"

namespace content {
class BrowserContext;
}

namespace views {
class DialogDelegate;
class NonClientFrameView;
class Widget;
class WidgetDelegate;
}

namespace web_modal {
class ModalDialogHost;
class WebContentsModalDialogHost;
}

// Update the position of dialog |widget| against |dialog_host|. This is used to
// reposition widgets e.g. when the host dimensions change.
void UpdateWebContentsModalDialogPosition(
    views::Widget* widget,
    web_modal::WebContentsModalDialogHost* dialog_host);

void UpdateBrowserModalDialogPosition(
    views::Widget* widget,
    web_modal::ModalDialogHost* dialog_host);

// Create a widget for |dialog| that is modal to the browser window |parent|.
// This places the dialog appropriately if |parent| is a valid browser window.
views::Widget* CreateBrowserModalDialogViews(views::DialogDelegate* dialog,
                                             gfx::NativeWindow parent);

views::NonClientFrameView* CreateConstrainedStyleNonClientFrameView(
    views::Widget* widget,
    content::BrowserContext* browser_context);

#endif  // CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_VIEWS_H_
