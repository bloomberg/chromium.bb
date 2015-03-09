// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/crw_js_message_dynamic_manager.h"

#include "base/logging.h"
#import "ios/web/web_state/js/crw_js_common_manager.h"

@implementation CRWJSMessageDynamicManager

- (NSString*)scriptPath {
  switch ([[self receiver] webViewType]) {
    case web::UI_WEB_VIEW_TYPE:
      return @"message_dynamic_ui";
    case web::WK_WEB_VIEW_TYPE:
      return @"message_dynamic_wk";
  }
  NOTREACHED();
  return nil;
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
