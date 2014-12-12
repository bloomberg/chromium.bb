// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/js/crw_js_base_manager.h"

@implementation CRWJSBaseManager

#pragma mark -
#pragma mark ProtectedMethods

- (NSString*)scriptPath {
  return @"base";
}

- (NSString*)presenceBeacon {
  return @"__gCrWeb";
}

@end
