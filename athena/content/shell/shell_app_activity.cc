// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/shell/shell_app_activity.h"

#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/views/controls/webview/webview.h"

namespace athena {

ShellAppActivity::ShellAppActivity(extensions::AppWindow* app_window)
    : AppActivity(app_window->extension_id()), app_window_(app_window) {
  DCHECK(app_window_);
}

ShellAppActivity::~ShellAppActivity() {
  app_window_->GetBaseWindow()->Close();  // Deletes |app_window_|.
}

views::Widget* ShellAppActivity::CreateWidget() {
  return NULL;  // Use default widget.
}

views::WebView* ShellAppActivity::GetWebView() {
  content::WebContents* web_contents = app_window_->web_contents();
  views::WebView* web_view =
      new views::WebView(web_contents->GetBrowserContext());
  web_view->SetWebContents(web_contents);
  Observe(web_contents);
  return web_view;
}

}  // namespace athena
