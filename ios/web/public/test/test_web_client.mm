// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/test_web_client.h"

#include "base/strings/sys_string_conversions.h"

namespace web {

TestWebClient::TestWebClient()
    : early_page_scripts_([[NSMutableDictionary alloc] init]) {
}

TestWebClient::~TestWebClient() {
}

std::string TestWebClient::GetUserAgent(bool desktop_user_agent) const {
  return desktop_user_agent ? desktop_user_agent_ : user_agent_;
}

void TestWebClient::SetUserAgent(const std::string& user_agent,
                                 bool is_desktop_user_agent) {
  if (is_desktop_user_agent)
    desktop_user_agent_ = user_agent;
  else
    user_agent_ = user_agent;
}

NSString* TestWebClient::GetEarlyPageScript(
    web::WebViewType web_view_type) const {
  NSString* result = [early_page_scripts_ objectForKey:@(web_view_type)];
  return result ? result : @"";
}

bool TestWebClient::WebViewsNeedActiveStateManager() const {
  return true;
}

void TestWebClient::SetEarlyPageScript(NSString* page_script,
                                       web::WebViewType web_view_type) {
  [early_page_scripts_ setObject:page_script forKey:@(web_view_type)];
}

}  // namespace web
