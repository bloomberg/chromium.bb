// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/shell/shell_app_activity.h"

#include "content/public/browser/web_contents.h"
#include "extensions/shell/browser/shell_app_window.h"
#include "ui/views/controls/webview/webview.h"

namespace athena {

ShellAppActivity::ShellAppActivity(extensions::ShellAppWindow* app_window,
                                   const std::string& app_id)
    : AppActivity(app_id), shell_app_window_(app_window) {
}

ShellAppActivity::~ShellAppActivity() {
}

views::Widget* ShellAppActivity::CreateWidget() {
  return NULL;  // Use default widget.
}

views::WebView* ShellAppActivity::GetWebView() {
  content::WebContents* web_contents =
      shell_app_window_->GetAssociatedWebContents();
  views::WebView* web_view =
      new views::WebView(web_contents->GetBrowserContext());
  web_view->SetWebContents(web_contents);
  Observe(web_contents);
  return web_view;
}

}  // namespace athena
