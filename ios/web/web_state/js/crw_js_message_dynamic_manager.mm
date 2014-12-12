// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/crw_js_message_dynamic_manager.h"

#import "ios/web/web_state/js/crw_js_common_manager.h"
#include "ios/web/web_view_util.h"

@implementation CRWJSMessageDynamicManager

- (NSString*)scriptPath {
  if (web::IsWKWebViewEnabled())
    return @"message_dynamic_wk";
  return @"message_dynamic_ui";
}

- (NSString*)presenceBeacon {
  return @"__gCrWeb.message_dynamic";
}

- (NSArray*)directDependencies {
  return @[
    // The 'base' manager is omitted deliberately; see <crbug.com/404640>.
    [CRWJSCommonManager class],
  ];
}

@end
