// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_client.h"

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/public/user_agent.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/web_view_early_page_script_provider.h"
#import "ios/web_view/internal/web_view_web_main_parts.h"
#include "ios/web_view/public/cwv_web_view.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewWebClient::WebViewWebClient() = default;

WebViewWebClient::~WebViewWebClient() = default;

std::unique_ptr<web::WebMainParts> WebViewWebClient::CreateWebMainParts() {
  return base::MakeUnique<WebViewWebMainParts>();
}

std::string WebViewWebClient::GetProduct() const {
  return base::SysNSStringToUTF8([CWVWebView userAgentProduct]);
}

std::string WebViewWebClient::GetUserAgent(web::UserAgentType type) const {
  return web::BuildUserAgentFromProduct(GetProduct());
}

base::StringPiece WebViewWebClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* WebViewWebClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

NSString* WebViewWebClient::GetEarlyPageScript(
    web::BrowserState* browser_state) const {
  return WebViewEarlyPageScriptProvider::FromBrowserState(browser_state)
      .GetScript();
}

}  // namespace ios_web_view
