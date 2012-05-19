// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/web_auth_flow_window_views.h"

#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

using content::BrowserContext;
using content::WebContents;
using views::View;
using views::WebView;
using views::Widget;
using views::WidgetDelegate;

WebAuthFlowWindowViews::WebAuthFlowWindowViews(
    Delegate* delegate,
    BrowserContext* browser_context,
    WebContents* contents)
    : WebAuthFlowWindow(delegate, browser_context, contents),
      web_view_(NULL),
      widget_(NULL) {
}

views::View* WebAuthFlowWindowViews::GetContentsView() {
  DCHECK(web_view_);
  return web_view_;
}

views::View* WebAuthFlowWindowViews::GetInitiallyFocusedView() {
  DCHECK(web_view_);
  return web_view_;
}

void WebAuthFlowWindowViews::DeleteDelegate() {
  if (delegate())
    delegate()->OnClose();
}

void WebAuthFlowWindowViews::Show() {
  web_view_ = new WebView(browser_context());
  web_view_->SetWebContents(contents());
  widget_ = Widget::CreateWindow(this);
  widget_->CenterWindow(gfx::Size(kDefaultWidth, kDefaultHeight));
  widget_->Show();
}

WebAuthFlowWindowViews::~WebAuthFlowWindowViews() {
  if (widget_)
    widget_->Close();  // This also deletes the widget.
}

// static
WebAuthFlowWindow* WebAuthFlowWindow::Create(
    Delegate* delegate,
    BrowserContext* browser_context,
    WebContents* contents) {
  return new WebAuthFlowWindowViews(delegate, browser_context, contents);
}
