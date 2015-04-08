// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/crw_js_early_script_manager.h"

#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/web_state/js/page_script_util.h"

@implementation CRWJSEarlyScriptManager

- (NSString*)staticInjectionContent {
  return web::GetEarlyPageScript(self.receiver.webViewType);
}

- (NSString*)presenceBeacon {
  return @"__gCrWeb";
}

@end
