// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_WEB_AUTH_FLOW_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_WEB_AUTH_FLOW_WINDOW_VIEWS_H_
#pragma once

#include "chrome/browser/ui/extensions/web_auth_flow_window.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget_delegate.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace views {
class View;
class WebView;
class Widget;
}

class WebAuthFlowWindowViews : public WebAuthFlowWindow,
                               public views::WidgetDelegateView {
 public:
  WebAuthFlowWindowViews(
      Delegate* delegate,
      content::BrowserContext* browser_context,
      content::WebContents* contents);

  // WidgetDelegate implementation.
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

  // WebAuthFlowWindow implementation.
  virtual void Show() OVERRIDE;

 private:
  virtual ~WebAuthFlowWindowViews();

  views::WebView* web_view_;
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthFlowWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_WEB_AUTH_FLOW_WINDOW_VIEWS_H_
