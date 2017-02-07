// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/web_contents/web_coordinator.h"

#import "ios/clean/chrome/browser/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/web_contents/web_contents_mediator.h"
#import "ios/clean/chrome/browser/ui/web_contents/web_contents_view_controller.h"
#import "ios/shared/chrome/browser/coordinator_context/coordinator_context.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WebCoordinator ()
@property(nonatomic, strong) WebContentsViewController* viewController;
@property(nonatomic, strong) WebContentsMediator* mediator;
@end

@implementation WebCoordinator
@synthesize webState = _webState;
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

- (instancetype)init {
  if ((self = [super init])) {
    _mediator = [[WebContentsMediator alloc] init];
  }
  return self;
}

- (void)setWebState:(web::WebState*)webState {
  _webState = webState;
  self.mediator.webState = self.webState;
}

- (void)start {
  self.viewController = [[WebContentsViewController alloc] init];
  self.mediator.consumer = self.viewController;

  // Reminder: this is a no-op if |baseViewController| is nil, for example
  // when this coordinator's view controller will be contained instead of
  // presented.
  [self.context.baseViewController presentViewController:self.viewController
                                                animated:self.context.animated
                                              completion:nil];
}

- (void)stop {
  // PLACEHOLDER: This is how the webUsageEnabled is set to false. Find a
  // better way in the future.
  self.mediator.webState = nullptr;
}

@end
