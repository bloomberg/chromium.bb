// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"

namespace chrome {

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
gfx::NativeWindow ShowWebDialog(gfx::NativeWindow parent,
                                content::BrowserContext* context,
                                ui::WebDialogDelegate* delegate) {
  views::Widget* widget = NULL;
  if (parent) {
    widget = views::Widget::CreateWindowWithParent(
        new views::WebDialogView(context,
                                 delegate,
                                 new ChromeWebContentsHandler),
        parent);
  } else {
    // We shouldn't be called with a NULL parent, but sometimes are.
    widget = views::Widget::CreateWindow(
        new views::WebDialogView(context,
                                 delegate,
                                 new ChromeWebContentsHandler));
  }

  widget->Show();
  return widget->GetNativeWindow();
}

}  // namespace chrome
