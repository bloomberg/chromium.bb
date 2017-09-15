// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/main_coordinator.h"

#import "ios/chrome/browser/ui/main/main_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface MainCoordinator () {
  // Instance variables backing properties of the same name.
  // |_mainViewController| will be owned by |self.window|.
  __weak MainViewController* _mainViewController;
}

@end

@implementation MainCoordinator

#pragma mark - property implementation.

- (MainViewController*)mainViewController {
  return _mainViewController;
}

#pragma mark - ChromeCoordinator implementation.

- (void)start {
  MainViewController* mainViewController = [[MainViewController alloc] init];
  _mainViewController = mainViewController;
  self.window.rootViewController = self.mainViewController;
}

@end
