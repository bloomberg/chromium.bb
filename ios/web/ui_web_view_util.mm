// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/ui_web_view_util.h"

namespace web {

void RegisterUserAgentForUIWebView(NSString* user_agent) {
  [[NSUserDefaults standardUserDefaults] registerDefaults:@{
    @"UserAgent" : user_agent
  }];
}

}  // namespace web
