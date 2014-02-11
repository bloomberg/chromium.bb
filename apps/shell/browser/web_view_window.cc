// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/web_view_window.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/aura/window.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// A simple window containing a single web view.
class WebViewWindowContents : public views::WidgetDelegateView {
 public:
  explicit WebViewWindowContents(content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}
  virtual ~WebViewWindowContents() {}

  // views::WidgetDelegateView overrides:
  virtual views::View* GetContentsView() OVERRIDE;
  virtual void WindowClosing() OVERRIDE;

  // views::View overrides:
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;

 private:
  // Initialize this view's children.
  void InitChildViews();

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(WebViewWindowContents);
};

views::View* WebViewWindowContents::GetContentsView() {
  return this;
}

void WebViewWindowContents::WindowClosing() {
  // Close the app when the window is closed.
  if (base::MessageLoopForUI::current()->is_running())
    base::MessageLoopForUI::current()->Quit();
}

void WebViewWindowContents::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  // Initialize child views when this view is attached.
  if (details.is_add && details.child == this)
    InitChildViews();
}

void WebViewWindowContents::InitChildViews() {
  // Create a WebView that fills the window.
  SetLayoutManager(new views::FillLayout);
  views::WebView* web_view = new views::WebView(browser_context_);
  AddChildView(web_view);

  web_view->LoadInitialURL(GURL("http://www.google.com/"));
  web_view->web_contents()->GetView()->Focus();
}

}  // namespace

namespace apps {

views::Widget* CreateWebViewWindow(content::BrowserContext* browser_context,
                                 aura::Window* window_context) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.delegate = new WebViewWindowContents(browser_context);
  params.context = window_context;
  // Make widget owns the native_widget, so that the pointer of widget is still
  // valid after user click the close button of the widget inside root window.
  // app_shell client controls the widget lifetime.
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = window_context->bounds();
  params.top_level = true;
  widget->Init(params);
  return widget;
}

}  // namespace apps
