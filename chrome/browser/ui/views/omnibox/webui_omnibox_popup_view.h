// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_WEBUI_OMNIBOX_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_WEBUI_OMNIBOX_POPUP_VIEW_H_

#include "base/macros.h"
#include "ui/views/controls/webview/webview.h"

class OmniboxPopupHandler;

// A WebView to display suggestions in the Views autocomplete popup.
class WebUIOmniboxPopupView : public views::WebView {
 public:
  explicit WebUIOmniboxPopupView(content::BrowserContext* browser_context);
  ~WebUIOmniboxPopupView() override = default;

  OmniboxPopupHandler* GetWebUIHandler();

  DISALLOW_COPY_AND_ASSIGN(WebUIOmniboxPopupView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_WEBUI_OMNIBOX_POPUP_VIEW_H_
