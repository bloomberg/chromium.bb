// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_PARTS_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_PARTS_H_

#include "ios/web/public/app/web_main_parts.h"

#include <memory>

@protocol CWVDelegate;

namespace ios_web_view {
class WebViewBrowserState;

// WebView implementation of WebMainParts.
class WebViewWebMainParts : public web::WebMainParts {
 public:
  explicit WebViewWebMainParts(id<CWVDelegate> delegate);
  ~WebViewWebMainParts() override;

  // WebMainParts implementation.
  void PreMainMessageLoopRun() override;

  // Returns the WebViewBrowserState for this embedder.
  WebViewBrowserState* browser_state() const { return browser_state_.get(); }

  // Returns the off the record WebViewBrowserState for this embedder.
  WebViewBrowserState* off_the_record_browser_state() const {
    return off_the_record_browser_state_.get();
  }

 private:
  // This object's delegate.
  __weak id<CWVDelegate> delegate_;

  // The BrowserState for this embedder.
  std::unique_ptr<WebViewBrowserState> browser_state_;

  // Off The Record BrowserState for this embedder.
  std::unique_ptr<WebViewBrowserState> off_the_record_browser_state_;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_PARTS_H_
