// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_client.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/web/public/user_agent.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/web_view_web_main_parts.h"
#import "ios/web_view/public/cwv_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewWebClient::WebViewWebClient(id<CWVDelegate> delegate)
    : delegate_(delegate), web_main_parts_(nullptr) {}

WebViewWebClient::~WebViewWebClient() = default;

web::WebMainParts* WebViewWebClient::CreateWebMainParts() {
  web_main_parts_ = new WebViewWebMainParts(delegate_);
  return web_main_parts_;
}

WebViewBrowserState* WebViewWebClient::browser_state() const {
  return web_main_parts_->browser_state();
}

WebViewBrowserState* WebViewWebClient::off_the_record_browser_state() const {
  return web_main_parts_->off_the_record_browser_state();
}

std::string WebViewWebClient::GetProduct() const {
  return base::SysNSStringToUTF8([delegate_ partialUserAgent]);
}

std::string WebViewWebClient::GetUserAgent(web::UserAgentType type) const {
  return web::BuildUserAgentFromProduct(GetProduct());
}

}  // namespace ios_web_view
