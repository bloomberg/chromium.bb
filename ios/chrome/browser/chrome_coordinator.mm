// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/chrome_coordinator.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ChromeCoordinator () {
  __weak UIViewController* _baseViewController;
  MutableCoordinatorArray* _childCoordinators;
}
@end

@implementation ChromeCoordinator

- (nullable instancetype)initWithBaseViewController:
    (UIViewController*)viewController {
  if (self = [super init]) {
    _baseViewController = viewController;
    _childCoordinators = [MutableCoordinatorArray array];
  }
  return self;
}

- (nullable instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  [self stop];
}

- (MutableCoordinatorArray*)childCoordinators {
  return _childCoordinators;
}

- (ChromeCoordinator*)activeChildCoordinator {
  // By default the active child is the one most recently added to the child
  // array, but subclasses can override this.
  return self.childCoordinators.lastObject;
}

- (nullable UIViewController*)baseViewController {
  return _baseViewController;
}

- (void)start {
  // Default implementation does nothing.
}

- (void)stop {
  // Default implementation does nothing.
}

@end
