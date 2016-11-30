// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/root_coordinator.h"

#include "base/ios/weak_nsobject.h"
#include "base/logging.h"

@interface RootCoordinator () {
  base::WeakNSObject<UIWindow> _window;
}
@end

@implementation RootCoordinator

- (instancetype)initWithWindow:(UIWindow*)window {
  if ((self = [super initWithBaseViewController:nil])) {
    _window.reset(window);
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController {
  NOTREACHED();
  return nil;
}

#pragma mark - property implementation

- (nullable UIWindow*)window {
  return _window;
}

@end
