// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_coordinator.h"

#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/ui/dialogs/java_script_dialog_blocking_state.h"
#import "ios/clean/chrome/browser/ui/commands/java_script_dialog_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/dialog_blocking/java_script_dialog_blocking_confirmation_coordinator.h"
#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_mediator.h"
#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_request.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_service.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface JavaScriptDialogCoordinator ()<JavaScriptDialogDismissalCommands> {
  // Backing object for property of same name (from Subclassing category).
  DialogMediator* _mediator;
}

// The dispatcher to use for JavaScriptdialogDismissalCommands.
@property(nonatomic, readonly) id<JavaScriptDialogDismissalCommands>
    callableDispatcher;
// The request for this dialog.
@property(nonatomic, strong) JavaScriptDialogRequest* request;

@end

@implementation JavaScriptDialogCoordinator
@synthesize request = _request;
@dynamic callableDispatcher;

- (instancetype)initWithRequest:(JavaScriptDialogRequest*)request {
  DCHECK(request.webState);
  if ((self = [super init])) {
    _request = request;
  }
  return self;
}

#pragma mark - BrowserCoordinator

- (void)start {
  _mediator = [[JavaScriptDialogMediator alloc]
      initWithRequest:self.request
           dispatcher:self.callableDispatcher];
  [self.dispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(JavaScriptDialogDismissalCommands)];
  [super start];
  web::WebState* webState = self.request.webState;
  JavaScriptDialogBlockingState::CreateForWebState(webState);
  JavaScriptDialogBlockingState::FromWebState(webState)
      ->JavaScriptDialogWasShown();
}

- (void)stop {
  [self.dispatcher stopDispatchingToTarget:self];
  [super stop];
}

#pragma mark - OverlayCoordinator

- (void)cancelOverlay {
  [self.request finishRequestWithSuccess:NO userInput:nil];
}

#pragma mark - JavaScriptDialogDismisalCommands

- (void)dismissJavaScriptDialog {
  [self stop];
}

- (void)dismissJavaScriptDialogWithBlockingConfirmation {
  // Replace the current overlay with a confirmation overlay.  The confirmation
  // coordinator will handle running |request|'s callback when confirmed or
  // cancelled, so reset |request| here to prevent running the callback while an
  // overlay is still displayed.
  JavaScriptDialogBlockingConfirmationCoordinator* confirmationCoordinator =
      [[JavaScriptDialogBlockingConfirmationCoordinator alloc]
          initWithRequest:self.request];
  self.request = nil;
  OverlayServiceFactory::GetInstance()
      ->GetForBrowserState(self.browser->browser_state())
      ->ReplaceVisibleOverlay(confirmationCoordinator, self.browser);
}

@end

@implementation JavaScriptDialogCoordinator (DialogCoordinatorSubclassing)

- (DialogMediator*)mediator {
  return _mediator;
}

@end
