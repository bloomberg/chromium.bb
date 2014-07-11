// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/app_activity.h"

#include "apps/shell/browser/shell_app_window.h"
#include "athena/activity/public/activity_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"

namespace athena {

// TODO(mukai): specifies the same accelerators of WebActivity.
AppActivity::AppActivity(apps::ShellAppWindow* app_window)
    : app_window_(app_window), web_view_(NULL) {
  DCHECK(app_window_);
}

AppActivity::~AppActivity() {
}

ActivityViewModel* AppActivity::GetActivityViewModel() {
  return this;
}

void AppActivity::Init() {
}

SkColor AppActivity::GetRepresentativeColor() const {
  // TODO(sad): Compute the color from the favicon.
  return SK_ColorGRAY;
}

base::string16 AppActivity::GetTitle() const {
  return web_view_->GetWebContents()->GetTitle();
}

bool AppActivity::UsesFrame() const {
  return false;
}

views::View* AppActivity::GetContentsView() {
  if (!web_view_) {
    // TODO(oshima): use apps::NativeAppWindowViews
    content::WebContents* web_contents =
        app_window_->GetAssociatedWebContents();
    web_view_ = new views::WebView(web_contents->GetBrowserContext());
    web_view_->SetWebContents(web_contents);
    Observe(web_contents);
  }
  return web_view_;
}

void AppActivity::TitleWasSet(content::NavigationEntry* entry,
                              bool explicit_set) {
  ActivityManager::Get()->UpdateActivity(this);
}

void AppActivity::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  ActivityManager::Get()->UpdateActivity(this);
}

}  // namespace athena
