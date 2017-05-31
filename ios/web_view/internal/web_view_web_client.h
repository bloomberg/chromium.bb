// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_CLIENT_H_

#include <memory>

#include "base/compiler_specific.h"
#import "ios/web/public/web_client.h"

namespace ios_web_view {

// WebView implementation of WebClient.
class WebViewWebClient : public web::WebClient {
 public:
  WebViewWebClient();
  ~WebViewWebClient() override;

  // WebClient implementation.
  std::unique_ptr<web::WebMainParts> CreateWebMainParts() override;
  std::string GetProduct() const override;
  std::string GetUserAgent(web::UserAgentType type) const override;
  NSString* GetEarlyPageScript(web::BrowserState* browser_state) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewWebClient);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_CLIENT_H_
