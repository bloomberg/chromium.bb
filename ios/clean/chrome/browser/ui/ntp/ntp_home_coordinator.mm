// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_home_coordinator.h"

#import "ios/chrome/browser/ui/ntp/google_landing_controller.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_home_mediator.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator+internal.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPHomeCoordinator ()
@property(nonatomic, strong) NTPHomeMediator* mediator;
@property(nonatomic, strong) UIViewController* viewController;
@property(nonatomic, strong) GoogleLandingController* wrapperController;
@end

@implementation NTPHomeCoordinator
@synthesize mediator = _mediator;
@synthesize viewController = _viewController;
@synthesize wrapperController = _wrapperController;

- (void)start {
  // PLACEHOLDER: Re-using old view controllers for now.
  self.mediator = [[NTPHomeMediator alloc] init];
  self.mediator.dispatcher = static_cast<id>(self.browser->dispatcher());
  self.wrapperController = [[GoogleLandingController alloc]
          initWithLoader:self.mediator
            browserState:self.browser->browser_state()
                 focuser:self.mediator
      webToolbarDelegate:nil
                tabModel:nil];
  self.viewController = [[UIViewController alloc] init];
  self.viewController.view = [self.wrapperController view];
  [super start];
}

@end
