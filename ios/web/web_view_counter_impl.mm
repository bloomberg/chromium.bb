// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_view_counter_impl.h"

#include "base/logging.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_thread.h"

namespace web {

namespace {
// A key used to associate a WebViewCounter with a BrowserState.
const char kWebViewCounterKeyName[] = "web_view_counter";
}  // namespace

WebViewCounterImpl::WebViewCounterImpl() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);
}

WebViewCounterImpl::~WebViewCounterImpl() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);
}

// static
WebViewCounter* WebViewCounter::FromBrowserState(
    web::BrowserState* browser_state) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);
  DCHECK(browser_state);

  return WebViewCounterImpl::FromBrowserState(browser_state);
}

// static
WebViewCounterImpl* WebViewCounterImpl::FromBrowserState(
    web::BrowserState* browser_state) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);
  DCHECK(browser_state);

  if (!browser_state->GetUserData(kWebViewCounterKeyName)) {
    browser_state->SetUserData(kWebViewCounterKeyName,
                               new WebViewCounterImpl());
  }
  return static_cast<WebViewCounterImpl*>(
      browser_state->GetUserData(kWebViewCounterKeyName));
}

size_t WebViewCounterImpl::GetWKWebViewCount() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);
  return wk_web_view_counter_.Size();
}

void WebViewCounterImpl::InsertWKWebView(WKWebView* wk_web_view) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(WebThread::UI);
  DCHECK(wk_web_view);
  DCHECK([wk_web_view isKindOfClass:[WKWebView class]]);

  wk_web_view_counter_.Insert(wk_web_view);
}

}  // namespace web
