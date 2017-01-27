// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/toolbar/sc_toolbar_coordinator.h"

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SCToolbarCoordinator
@synthesize baseViewController = _baseViewController;

- (void)start {
  ToolbarViewController* viewController = [[ToolbarViewController alloc] init];
  viewController.title = @"Toolbar";
  [self.baseViewController pushViewController:viewController animated:YES];
}

@end
