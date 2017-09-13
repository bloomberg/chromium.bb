// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/web_contents/web_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper.h"
#import "ios/clean/chrome/browser/ui/commands/context_menu_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/context_menu/context_menu_dialog_coordinator.h"
#import "ios/clean/chrome/browser/ui/dialogs/context_menu/context_menu_dialog_request.h"
#import "ios/clean/chrome/browser/ui/dialogs/http_auth_dialogs/http_auth_dialog_coordinator.h"
#import "ios/clean/chrome/browser/ui/dialogs/http_auth_dialogs/http_auth_dialog_request.h"
#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_overlay_presenter.h"
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
  SadTabTabHelper* sadTabHelper = SadTabTabHelper::FromWebState(self.webState);
  // Set the mediator as a SadTabHelper delegate, and set the mediator's
  // dispatcher.
  if (sadTabHelper) {
    sadTabHelper->SetDelegate(self.mediator);
    self.mediator.dispatcher = static_cast<id>(self.dispatcher);
  }
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
  SadTabTabHelper* sadTabHelper = SadTabTabHelper::FromWebState(self.webState);
  if (sadTabHelper)
    sadTabHelper->SetDelegate(nil);
  [super stop];
}

- (void)childCoordinatorDidStart:(BrowserCoordinator*)childCoordinator {
  // Register to receive relevant ContextMenuCommands.
  if ([childCoordinator isKindOfClass:[ContextMenuDialogCoordinator class]]) {
    [self.dispatcher
        startDispatchingToTarget:self
                     forSelector:@selector(executeContextMenuScript:)];
    [self.dispatcher startDispatchingToTarget:self
                                  forSelector:@selector(openContextMenuImage:)];
  }
  [self.viewController presentViewController:childCoordinator.viewController
                                    animated:YES
                                  completion:nil];
}

- (void)childCoordinatorWillStop:(BrowserCoordinator*)childCoordinator {
  // Unregister ContextMenuCommands once the UI has been dismissed.
  if ([childCoordinator isKindOfClass:[ContextMenuDialogCoordinator class]]) {
    [self.dispatcher
        stopDispatchingForSelector:@selector(executeContextMenuScript:)];
    [self.dispatcher
        stopDispatchingForSelector:@selector(openContextMenuImage:)];
  }
}

#pragma mark - ContextMenuCommand

- (void)executeContextMenuScript:(ContextMenuDialogRequest*)request {
  self.webState->ExecuteJavaScript(request.script);
}

- (void)openContextMenuImage:(ContextMenuDialogRequest*)request {
  web::NavigationManager::WebLoadParams loadParams(request.imageURL);
  self.webState->GetNavigationManager()->LoadURLWithParams(loadParams);
}

#pragma mark - CRWWebStateDelegate

- (web::JavaScriptDialogPresenter*)javaScriptDialogPresenterForWebState:
    (web::WebState*)webState {
  DCHECK_EQ(self.webState, webState);
  JavaScriptDialogOverlayPresenter::CreateForBrowser(self.browser);
  return JavaScriptDialogOverlayPresenter::FromBrowser(self.browser);
}

- (void)webState:(web::WebState*)webState
    handleContextMenu:(const web::ContextMenuParams&)params {
  DCHECK_EQ(self.webState, webState);
  ContextMenuDialogRequest* request =
      [ContextMenuDialogRequest requestWithParams:params];
  ContextMenuDialogCoordinator* contextMenu =
      [[ContextMenuDialogCoordinator alloc] initWithRequest:request];
  OverlayServiceFactory::GetInstance()
      ->GetForBrowserState(self.browser->browser_state())
      ->ShowOverlayForWebState(contextMenu, self.webState);
}

- (void)webState:(web::WebState*)webState
    didRequestHTTPAuthForProtectionSpace:(NSURLProtectionSpace*)protectionSpace
                      proposedCredential:(NSURLCredential*)proposedCredential
                       completionHandler:(void (^)(NSString* username,
                                                   NSString* password))handler {
  HTTPAuthDialogRequest* request =
      [HTTPAuthDialogRequest requestWithWebState:webState
                                 protectionSpace:protectionSpace
                                      credential:proposedCredential
                                        callback:handler];
  HTTPAuthDialogCoordinator* dialogCoordinator =
      [[HTTPAuthDialogCoordinator alloc] initWithRequest:request];
  OverlayService* overlayService =
      OverlayServiceFactory::GetInstance()->GetForBrowserState(
          self.browser->browser_state());
  if (overlayService) {
    overlayService->ShowOverlayForWebState(dialogCoordinator, webState);
  } else {
    [dialogCoordinator cancelOverlay];
  }
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
