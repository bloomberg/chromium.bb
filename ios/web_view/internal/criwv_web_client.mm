// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/criwv_web_client.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/web/public/user_agent.h"
#include "ios/web_view/internal/criwv_browser_state.h"
#import "ios/web_view/internal/criwv_web_main_parts.h"
#import "ios/web_view/public/criwv_delegate.h"

namespace ios_web_view {

CRIWVWebClient::CRIWVWebClient(id<CRIWVDelegate> delegate)
    : delegate_(delegate),
      web_main_parts_(nullptr) {}

CRIWVWebClient::~CRIWVWebClient() {}

web::WebMainParts* CRIWVWebClient::CreateWebMainParts() {
  web_main_parts_ = new CRIWVWebMainParts(delegate_);
  return web_main_parts_;
}

CRIWVBrowserState* CRIWVWebClient::browser_state() const {
  return web_main_parts_->browser_state();
}

std::string CRIWVWebClient::GetProduct() const {
  return base::SysNSStringToUTF8([delegate_ partialUserAgent]);
}

std::string CRIWVWebClient::GetUserAgent(bool desktop_user_agent) const {
  std::string product = GetProduct();
  return web::BuildUserAgentFromProduct(product);
}

}  // namespace ios_web_view
