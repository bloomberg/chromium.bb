// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/top_level_widget.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/widget/widget.h"

namespace chrome {

views::Widget* GetTopLevelWidgetForBrowser(Browser* browser) {
  if (!browser)
    return NULL;

  BrowserWindow* window = browser->window();
  if (!window)
    return NULL;

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view ? browser_view->GetWidget() : NULL;
}

}  // namespace chrome
