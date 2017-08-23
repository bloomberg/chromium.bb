// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/web_contents/web_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/commands/context_menu_commands.h"
#import "ios/clean/chrome/browser/ui/context_menu/context_menu_context_impl.h"
#import "ios/clean/chrome/browser/ui/context_menu/web_context_menu_coordinator.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_service.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_service_factory.h"
#import "ios/clean/chrome/browser/ui/web_contents/web_contents_mediator.h"
#import "ios/clean/chrome/browser/ui/web_contents/web_contents_view_controller.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_delegate_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WebCoordinator ()<ContextMenuCommands, CRWWebStateDelegate> {
  std::unique_ptr<web::WebStateDelegateBridge> _webStateDelegate;
}
@property(nonatomic, strong) WebContentsViewController* viewController;
@property(nonatomic, strong) WebContentsMediator* mediator;

// Tells the OverlayService that this coordinator is presenting |webState|.
- (void)setWebStateOverlayParent;
// Tells the OverlayService that this coordinator is no longer presenting
// |webState|.
- (void)resetWebStateOverlayParent;

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
  [self resetWebStateOverlayParent];
  _webState = webState;
  self.webState->SetDelegate(_webStateDelegate.get());
  self.mediator.webState = self.webState;
  [self setWebStateOverlayParent];
}

- (void)start {
  // Create the view controller and start it.
  self.viewController = [[WebContentsViewController alloc] init];
  self.mediator.consumer = self.viewController;
  self.mediator.webState = self.webState;
  [super start];
  [self setWebStateOverlayParent];
}

- (void)stop {
  [self resetWebStateOverlayParent];
  [super stop];
}

- (void)childCoordinatorDidStart:(BrowserCoordinator*)childCoordinator {
  // Register to receive relevant ContextMenuCommands.
  if ([childCoordinator isKindOfClass:[WebContextMenuCoordinator class]]) {
    [self.browser->dispatcher()
        startDispatchingToTarget:self
                     forSelector:@selector(executeContextMenuScript:)];
    [self.browser->dispatcher()
        startDispatchingToTarget:self
                     forSelector:@selector(openContextMenuImage:)];
  }
  [self.viewController presentViewController:childCoordinator.viewController
                                    animated:YES
                                  completion:nil];
}

- (void)childCoordinatorWillStop:(BrowserCoordinator*)childCoordinator {
  // Unregister ContextMenuCommands once the UI has been dismissed.
  if ([childCoordinator isKindOfClass:[WebContextMenuCoordinator class]]) {
    [self.browser->dispatcher()
        stopDispatchingForSelector:@selector(executeContextMenuScript:)];
    [self.browser->dispatcher()
        stopDispatchingForSelector:@selector(openContextMenuImage:)];
  }
}

#pragma mark - ContextMenuCommand

- (void)executeContextMenuScript:(ContextMenuContext*)context {
  ContextMenuContextImpl* contextImpl =
      base::mac::ObjCCastStrict<ContextMenuContextImpl>(context);
  self.webState->ExecuteJavaScript(contextImpl.script);
}

- (void)openContextMenuImage:(ContextMenuContext*)context {
  ContextMenuContextImpl* contextImpl =
      base::mac::ObjCCastStrict<ContextMenuContextImpl>(context);
  web::NavigationManager::WebLoadParams loadParams(contextImpl.imageURL);
  self.webState->GetNavigationManager()->LoadURLWithParams(loadParams);
}

#pragma mark - CRWWebStateDelegate

- (BOOL)webState:(web::WebState*)webState
    handleContextMenu:(const web::ContextMenuParams&)params {
  ContextMenuContextImpl* context =
      [[ContextMenuContextImpl alloc] initWithParams:params];
  WebContextMenuCoordinator* contextMenu =
      [[WebContextMenuCoordinator alloc] initWithContext:context];
  [self addChildCoordinator:contextMenu];
  [contextMenu start];
  return YES;
}

#pragma mark -

- (void)setWebStateOverlayParent {
  if (self.webState && self.started && self.browser) {
    OverlayServiceFactory::GetInstance()
        ->GetForBrowserState(self.browser->browser_state())
        ->SetWebStateParentCoordinator(self, self.webState);
  }
}

- (void)resetWebStateOverlayParent {
  if (self.webState && self.browser) {
    OverlayServiceFactory::GetInstance()
        ->GetForBrowserState(self.browser->browser_state())
        ->SetWebStateParentCoordinator(nil, self.webState);
  }
}

@end
