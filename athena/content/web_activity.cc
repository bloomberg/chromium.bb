// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/web_activity.h"

#include "athena/activity/public/activity_manager.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"

namespace athena {

WebActivity::WebActivity(content::BrowserContext* browser_context,
                         const GURL& url)
    : browser_context_(browser_context), url_(url), web_view_(NULL) {
}

WebActivity::~WebActivity() {
}

ActivityViewModel* WebActivity::GetActivityViewModel() {
  return this;
}

SkColor WebActivity::GetRepresentativeColor() {
  // TODO(sad): Compute the color from the favicon.
  return SK_ColorGRAY;
}

std::string WebActivity::GetTitle() {
  return base::UTF16ToUTF8(web_view_->GetWebContents()->GetTitle());
}

views::View* WebActivity::GetContentsView() {
  if (!web_view_) {
    web_view_ = new views::WebView(browser_context_);
    web_view_->LoadInitialURL(url_);
    Observe(web_view_->GetWebContents());
  }
  return web_view_;
}

void WebActivity::TitleWasSet(content::NavigationEntry* entry,
                              bool explicit_set) {
  ActivityManager::Get()->UpdateActivity(this);
}

void WebActivity::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  ActivityManager::Get()->UpdateActivity(this);
}

}  // namespace athena
