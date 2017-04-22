// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_home_coordinator.h"

#import "ios/chrome/browser/ui/ntp/google_landing_controller.h"
#import "ios/chrome/browser/ui/ntp/google_landing_mediator.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_home_mediator.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator+internal.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPHomeCoordinator ()
@property(nonatomic, strong) NTPHomeMediator* mediator;
@property(nonatomic, strong) GoogleLandingMediator* googleLandingMediator;
@property(nonatomic, strong) GoogleLandingController* viewController;
@end

@implementation NTPHomeCoordinator
@synthesize mediator = _mediator;
@synthesize googleLandingMediator = _googleLandingMediator;
@synthesize viewController = _viewController;

- (void)start {
  // PLACEHOLDER: self.mediator and self.oldMediator should be merged together.
  self.mediator = [[NTPHomeMediator alloc] init];
  self.mediator.dispatcher = static_cast<id>(self.browser->dispatcher());
  self.viewController = [[GoogleLandingController alloc] init];
  self.googleLandingMediator = [[GoogleLandingMediator alloc]
        initWithConsumer:self.viewController
            browserState:self.browser->browser_state()
                  loader:self.mediator
                 focuser:self.mediator
      webToolbarDelegate:nil
            webStateList:&self.browser->web_state_list()];
  self.viewController.dataSource = self.googleLandingMediator;
  [super start];
}

@end
