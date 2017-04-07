// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/web_contents/web_coordinator.h"

#include "base/memory/ptr_util.h"
#import "ios/clean/chrome/browser/ui/context_menu/web_context_menu_coordinator.h"
#import "ios/clean/chrome/browser/ui/web_contents/web_contents_mediator.h"
#import "ios/clean/chrome/browser/ui/web_contents/web_contents_view_controller.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#include "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_delegate_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WebCoordinator ()<CRWWebStateDelegate> {
  std::unique_ptr<web::WebStateDelegateBridge> _webStateDelegate;
}
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
    _webStateDelegate = base::MakeUnique<web::WebStateDelegateBridge>(self);
  }
  return self;
}

- (void)setWebState:(web::WebState*)webState {
  // PLACEHOLDER: The web state delegate will be set by another object, and
  // this coordinator will not need to know the active web state.
  _webState = webState;
  self.webState->SetDelegate(_webStateDelegate.get());
}

- (void)start {
  self.viewController = [[WebContentsViewController alloc] init];
  self.mediator.consumer = self.viewController;
  self.mediator.webStateList = &self.browser->web_state_list();
  [super start];
}

- (void)stop {
  [super stop];
  [self.mediator disconnect];
}

- (void)childCoordinatorDidStart:(BrowserCoordinator*)childCoordinator {
  DCHECK([childCoordinator isKindOfClass:[WebContextMenuCoordinator class]]);
  [self.viewController presentViewController:childCoordinator.viewController
                                    animated:YES
                                  completion:nil];
}

#pragma mark - CRWWebStateDelegate

- (BOOL)webState:(web::WebState*)webState
    handleContextMenu:(const web::ContextMenuParams&)params {
  WebContextMenuCoordinator* contextMenu =
      [[WebContextMenuCoordinator alloc] init];
  [self addChildCoordinator:contextMenu];
  [contextMenu start];
  return YES;
}

@end
