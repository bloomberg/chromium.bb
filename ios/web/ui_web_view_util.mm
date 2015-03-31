// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/ui_web_view_util.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/net/request_group_util.h"
#include "ios/web/public/web_client.h"

namespace {
// Returns true if given Request Group ID string contains only decimal digits.
BOOL IsValidRequestGroupID(NSString* request_group_id) {
  return [[NSCharacterSet decimalDigitCharacterSet] isSupersetOfSet:
      [NSCharacterSet characterSetWithCharactersInString:request_group_id]];
}
}  // namespace

namespace web {

void BuildAndRegisterUserAgentForUIWebView(NSString* request_group_id,
                                           BOOL use_desktop_user_agent) {
  DCHECK(!request_group_id || IsValidRequestGroupID(request_group_id));
  DCHECK(web::GetWebClient());
  std::string baseUserAgent = web::GetWebClient()->GetUserAgent(
      use_desktop_user_agent);
  web::RegisterUserAgentForUIWebView(
      web::AddRequestGroupIDToUserAgent(base::SysUTF8ToNSString(baseUserAgent),
                                        request_group_id));
}

void RegisterUserAgentForUIWebView(NSString* user_agent) {
  [[NSUserDefaults standardUserDefaults] registerDefaults:@{
    @"UserAgent" : user_agent
  }];
}

}  // namespace web
