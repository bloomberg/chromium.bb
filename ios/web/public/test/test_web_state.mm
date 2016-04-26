// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/test_web_state.h"

#include <stdint.h>

#include "base/callback.h"

namespace web {

TestWebState::TestWebState()
    : web_usage_enabled_(false),
      trust_level_(kAbsolute),
      content_is_html_(true) {}

TestWebState::~TestWebState() = default;

WebStateDelegate* TestWebState::GetDelegate() {
  return nil;
}

void TestWebState::SetDelegate(WebStateDelegate* delegate) {}

BrowserState* TestWebState::GetBrowserState() const {
  return nullptr;
}

bool TestWebState::IsWebUsageEnabled() const {
  return web_usage_enabled_;
}

void TestWebState::SetWebUsageEnabled(bool enabled) {
  web_usage_enabled_ = enabled;
}

UIView* TestWebState::GetView() {
  return nullptr;
}

NavigationManager* TestWebState::GetNavigationManager() {
  return nullptr;
}

CRWJSInjectionReceiver* TestWebState::GetJSInjectionReceiver() const {
  return nullptr;
}

void TestWebState::ExecuteJavaScript(const base::string16& javascript) {}

void TestWebState::ExecuteJavaScript(const base::string16& javascript,
                                     const JavaScriptResultCallback& callback) {
  callback.Run(nullptr);
}

const std::string& TestWebState::GetContentsMimeType() const {
  return mime_type_;
}

const std::string& TestWebState::GetContentLanguageHeader() const {
  return content_language_;
}

bool TestWebState::ContentIsHTML() const {
  return content_is_html_;
}

const GURL& TestWebState::GetVisibleURL() const {
  return url_;
}

const GURL& TestWebState::GetLastCommittedURL() const {
  return url_;
}

GURL TestWebState::GetCurrentURL(URLVerificationTrustLevel* trust_level) const {
  *trust_level = trust_level_;
  return url_;
}

bool TestWebState::IsShowingWebInterstitial() const {
  return false;
}

WebInterstitial* TestWebState::GetWebInterstitial() const {
  return nullptr;
}

int TestWebState::GetCertGroupId() const {
  return 0;
}

void TestWebState::SetContentIsHTML(bool content_is_html) {
  content_is_html_ = content_is_html;
}

const base::string16& TestWebState::GetTitle() const {
  return title_;
}

bool TestWebState::IsLoading() const {
  return false;
}

double TestWebState::GetLoadingProgress() const {
  return 0.0;
}

bool TestWebState::IsBeingDestroyed() const {
  return false;
}

void TestWebState::SetCurrentURL(const GURL& url) {
  url_ = url;
}

void TestWebState::SetTrustLevel(URLVerificationTrustLevel trust_level) {
  trust_level_ = trust_level;
}

CRWWebViewProxyType TestWebState::GetWebViewProxy() const {
  return nullptr;
}

int TestWebState::DownloadImage(const GURL& url,
                                bool is_favicon,
                                uint32_t max_bitmap_size,
                                bool bypass_cache,
                                const ImageDownloadCallback& callback) {
  return 0;
}

base::WeakPtr<WebState> TestWebState::AsWeakPtr() {
  NOTREACHED();
  return base::WeakPtr<WebState>();
}

}  // namespace web
