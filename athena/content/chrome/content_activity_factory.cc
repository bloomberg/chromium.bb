// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/content_activity_factory.h"

#include "athena/content/app_activity.h"
#include "extensions/browser/app_window/app_window.h"
#include "ui/views/controls/webview/webview.h"

// TODO(oshima): Consolidate this and app shell implementation once
// crbug.com/403726 is fixed.
namespace athena {
namespace {

class ChromeAppActivity : public AppActivity {
 public:
  ChromeAppActivity(extensions::AppWindow* app_window, views::WebView* web_view)
      : AppActivity(app_window->extension_id()),
        app_window_(app_window),
        web_view_(web_view) {
    DCHECK_EQ(app_window->web_contents(), web_view->GetWebContents());
    Observe(app_window_->web_contents());
  }

 private:
  virtual ~ChromeAppActivity() {}

  // ActivityViewModel overrides:
  virtual views::Widget* CreateWidget() OVERRIDE {
    // This is necessary to register apps.
    // TODO(oshima): This will become unnecessary once the
    // shell_app_window is removed.
    GetContentsView();
    return web_view_->GetWidget();
  }

  // AppActivity:
  virtual views::WebView* GetWebView() OVERRIDE { return web_view_; }

  // Not owned.
  extensions::AppWindow* app_window_;
  views::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppActivity);
};

}  // namespace

Activity* ContentActivityFactory::CreateAppActivity(
    extensions::AppWindow* app_window,
    views::WebView* web_view) {
  return new ChromeAppActivity(app_window, web_view);
}

Activity* ContentActivityFactory::CreateAppActivity(
    extensions::ShellAppWindow* app_window,
    const std::string& app_id) {
  return NULL;
}

}  // namespace athena
