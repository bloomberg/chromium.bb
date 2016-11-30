// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/chrome_coordinator.h"

#include "base/ios/weak_nsobject.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"

@interface ChromeCoordinator () {
  base::WeakNSObject<UIViewController> _baseViewController;
  base::scoped_nsobject<MutableCoordinatorArray> _childCoordinators;
}
@end

@implementation ChromeCoordinator

- (nullable instancetype)initWithBaseViewController:
    (UIViewController*)viewController {
  if (self = [super init]) {
    _baseViewController.reset(viewController);
    _childCoordinators.reset([[MutableCoordinatorArray array] retain]);
  }
  return self;
}

- (nullable instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  [self stop];
  [super dealloc];
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
