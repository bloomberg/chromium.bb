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
