// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/omnibox/location_bar_coordinator.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/ui/omnibox/location_bar_controller_impl.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"
#import "ios/clean/chrome/browser/ui/omnibox/location_bar_mediator.h"
#import "ios/clean/chrome/browser/ui/omnibox/location_bar_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LocationBarCoordinator ()

@property(nonatomic, strong) LocationBarMediator* mediator;
@property(nonatomic, readwrite, strong)
    LocationBarViewController* viewController;

@end

@implementation LocationBarCoordinator {
}

@synthesize mediator = _mediator;
@synthesize viewController = _viewController;

- (void)start {
  DCHECK(self.parentCoordinator);
  self.viewController = [[LocationBarViewController alloc] init];
  Browser* browser = self.browser;

  // Begin broadcasting the omnibox frame.
  [browser->broadcaster() broadcastValue:@"omniboxFrame"
                                ofObject:self.viewController
                                selector:@selector(broadcastOmniboxFrame:)];

  self.mediator = [[LocationBarMediator alloc]
      initWithWebStateList:&(browser->web_state_list())];
  std::unique_ptr<LocationBarController> locationBar =
      base::MakeUnique<LocationBarControllerImpl>(
          self.viewController.omnibox, browser->browser_state(), self.mediator,
          nil /* dispatcher */);
  [self.mediator setLocationBar:std::move(locationBar)];
  [super start];
}

- (void)stop {
  [super stop];

  [self.browser->broadcaster()
      stopBroadcastingForSelector:@selector(broadcastOmniboxFrame:)];
  self.viewController = nil;
  self.mediator = nil;
}

@end
