// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/main_coordinator.h"

#include "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/main/main_view_controller.h"

@interface MainCoordinator () {
  // Instance variables backing properties of the same name.
  // |_mainViewController| will be owned by |self.window|.
  base::WeakNSObject<MainViewController> _mainViewController;
}

@end

@implementation MainCoordinator

#pragma mark - property implementation.

- (MainViewController*)mainViewController {
  return _mainViewController;
}

#pragma mark - ChromeCoordinator implementation.

- (void)start {
  base::scoped_nsobject<MainViewController> mainViewController(
      [[MainViewController alloc] init]);
  _mainViewController.reset(mainViewController);
  self.window.rootViewController = self.mainViewController;

  // Size the main view controller to fit the whole screen.
  [self.mainViewController.view setFrame:self.window.bounds];
}

@end
