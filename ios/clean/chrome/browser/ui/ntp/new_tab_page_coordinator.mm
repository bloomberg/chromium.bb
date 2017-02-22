// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#import "ios/clean/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/shared/chrome/browser/coordinator_context/coordinator_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPCoordinator ()
@property(nonatomic, strong) NTPViewController* viewController;
@end

@implementation NTPCoordinator
@synthesize viewController = _viewController;

- (void)start {
  self.viewController = [[NTPViewController alloc] init];
  [self.context.baseViewController presentViewController:self.viewController
                                                animated:self.context.animated
                                              completion:nil];
}

@end
