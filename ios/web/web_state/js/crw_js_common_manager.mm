// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/js/crw_js_common_manager.h"

#import "ios/web/public/web_state/js/crw_js_base_manager.h"

@implementation CRWJSCommonManager

#pragma mark -
#pragma mark ProtectedMethods

- (NSString*)scriptPath {
  return @"common";
}

- (NSString*)presenceBeacon {
  return @"__gCrWeb.common";
}

- (NSArray*)directDependencies {
  return @[ [CRWJSBaseManager class] ];
}

@end
