// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_home_coordinator.h"

#import "ios/chrome/browser/ui/ntp/google_landing_mediator.h"
#import "ios/chrome/browser/ui/ntp/google_landing_view_controller.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_home_mediator.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/shared/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator+internal.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPHomeCoordinator ()
@property(nonatomic, strong) NTPHomeMediator* mediator;
@property(nonatomic, strong) GoogleLandingMediator* googleLandingMediator;
@property(nonatomic, strong) GoogleLandingViewController* viewController;
@end

@implementation NTPHomeCoordinator
@synthesize mediator = _mediator;
@synthesize googleLandingMediator = _googleLandingMediator;
@synthesize viewController = _viewController;

- (void)start {
  // PLACEHOLDER: self.mediator and self.oldMediator should be merged together.
  self.mediator = [[NTPHomeMediator alloc] init];
  self.mediator.dispatcher = static_cast<id>(self.browser->dispatcher());

  // PLACEHOLDER: These will go elsewhere.
  [self.browser->dispatcher() startDispatchingToTarget:self.mediator
                                           forProtocol:@protocol(UrlLoader)];
  [self.browser->dispatcher()
      startDispatchingToTarget:self.mediator
                   forProtocol:@protocol(OmniboxFocuser)];
  self.viewController = [[GoogleLandingViewController alloc] init];
  self.viewController.dispatcher = static_cast<id>(self.browser->dispatcher());
  self.googleLandingMediator = [[GoogleLandingMediator alloc]
      initWithConsumer:self.viewController
          browserState:self.browser->browser_state()
            dispatcher:static_cast<id>(self.browser->dispatcher())
          webStateList:&self.browser->web_state_list()];
  self.viewController.dataSource = self.googleLandingMediator;
  [super start];
}

- (void)stop {
  [super stop];
  [self.googleLandingMediator shutdown];

  [self.browser->dispatcher() stopDispatchingForProtocol:@protocol(UrlLoader)];
  [self.browser->dispatcher()
      stopDispatchingForProtocol:@protocol(OmniboxFocuser)];
}

@end
