// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_wk_navigation_handler.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWWKNavigationHandler

- (instancetype)init {
  if (self = [super init]) {
    _navigationStates = [[CRWWKNavigationStates alloc] init];
  }
  return self;
}

@end
