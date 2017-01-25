// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/coordinator_context/coordinator_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CoordinatorContext

@synthesize baseViewController = _baseViewController;
@synthesize animated = _animated;

- (instancetype)init {
  self = [super init];
  if (self) {
    _animated = YES;
  }
  return self;
}

@end
