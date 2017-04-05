// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_client.h"

#include "ios/web/public/user_agent.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/web_view_early_page_script_provider.h"
#import "ios/web_view/internal/web_view_web_main_parts.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewWebClient::WebViewWebClient(const std::string& user_agent_product)
    : user_agent_product_(user_agent_product), web_main_parts_(nullptr) {}

WebViewWebClient::~WebViewWebClient() = default;

web::WebMainParts* WebViewWebClient::CreateWebMainParts() {
  web_main_parts_ = new WebViewWebMainParts();
  return web_main_parts_;
}

WebViewBrowserState* WebViewWebClient::browser_state() const {
  return web_main_parts_->browser_state();
}

WebViewBrowserState* WebViewWebClient::off_the_record_browser_state() const {
  return web_main_parts_->off_the_record_browser_state();
}

std::string WebViewWebClient::GetProduct() const {
  return user_agent_product_;
}

std::string WebViewWebClient::GetUserAgent(web::UserAgentType type) const {
  return web::BuildUserAgentFromProduct(GetProduct());
}

NSString* WebViewWebClient::GetEarlyPageScript(
    web::BrowserState* browser_state) const {
  return WebViewEarlyPageScriptProvider::FromBrowserState(browser_state)
      .GetScript();
}

}  // namespace ios_web_view
