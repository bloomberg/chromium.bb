// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"

namespace chrome {

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
gfx::NativeWindow ShowWebDialog(gfx::NativeView parent,
                                content::BrowserContext* context,
                                ui::WebDialogDelegate* delegate) {
  views::WebDialogView* view =
      new views::WebDialogView(context, delegate, new ChromeWebContentsHandler);
  // NOTE: The |parent| may be null, which will result in the default window
  // placement on Aura.
  views::Widget* widget = views::Widget::CreateWindowWithParent(view, parent);

  // Observer is needed for ChromeVox extension to send messages between content
  // and background scripts.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      view->web_contents());

  widget->Show();
  return widget->GetNativeWindow();
}

}  // namespace chrome
