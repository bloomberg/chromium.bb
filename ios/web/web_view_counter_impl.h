// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_VIEW_COUNTER_IMPL_H_
#define IOS_WEB_WEB_VIEW_COUNTER_IMPL_H_

#import <WebKit/WebKit.h>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "ios/web/public/web_view_counter.h"
#import "ios/web/weak_nsobject_counter.h"

namespace web {

class WebViewCounterImpl : public WebViewCounter,
                           public base::SupportsUserData::Data {
 public:
  WebViewCounterImpl();
  ~WebViewCounterImpl() override;

  // Returns the WebViewCounterImpl for the given |browser_state|. Lazily
  // attaches one if it does not exist. |browser_state| cannot be null.
  static web::WebViewCounterImpl* FromBrowserState(
      web::BrowserState* browser_state);

  // WebViewCounter methods.
  size_t GetWKWebViewCount() override;

  // Inserts a WKWebView for tracking. |wk_web_view| cannot be null.
  void InsertWKWebView(WKWebView* wk_web_view);

 private:
  // The WeakNSObjectCounter that counts WKWebViews.
  WeakNSObjectCounter wk_web_view_counter_;

  DISALLOW_COPY_AND_ASSIGN(WebViewCounterImpl);
};

}  // namespace web

#endif  // IOS_WEB_WEB_VIEW_COUNTER_IMPL_H_
