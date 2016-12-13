// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/root_coordinator.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation RootCoordinator
@synthesize window = _window;

- (instancetype)initWithWindow:(UIWindow*)window {
  if ((self = [super initWithBaseViewController:nil])) {
    _window = window;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController {
  NOTREACHED();
  return nil;
}

@end
