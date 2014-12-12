// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/js/crw_js_message_manager.h"

#import "ios/web/web_state/js/crw_js_common_manager.h"
#import "ios/web/web_state/js/crw_js_message_dynamic_manager.h"

@implementation CRWJSMessageManager

- (NSString*)scriptPath {
  return @"message";
}

- (NSString*)presenceBeacon {
  return @"__gCrWeb.message";
}

- (NSArray*)directDependencies {
  // The 'base' manager is omitted deliberately; see <crbug.com/404640>.
  return @[ [CRWJSCommonManager class], [CRWJSMessageDynamicManager class] ];
}

@end
