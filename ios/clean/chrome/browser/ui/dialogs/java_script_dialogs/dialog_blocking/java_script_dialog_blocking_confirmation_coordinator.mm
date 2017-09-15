// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/dialog_blocking/java_script_dialog_blocking_confirmation_coordinator.h"

#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/commands/dialog_commands.h"
#import "ios/clean/chrome/browser/ui/commands/java_script_dialog_blocking_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/dialog_blocking/java_script_dialog_blocking_confirmation_mediator.h"
#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface JavaScriptDialogBlockingConfirmationCoordinator ()<
    JavaScriptDialogBlockingDismissalCommands> {
  // Backing object for property of same name (from Subclassing category).
  DialogMediator* _mediator;
}

// The dispatcher to use for JavaScriptDialogBlockingDismissalCommands.
@property(nonatomic, readonly) id<JavaScriptDialogBlockingDismissalCommands>
    callableDispatcher;
// The request for this dialog.
@property(nonatomic, strong) JavaScriptDialogRequest* request;

@end

@implementation JavaScriptDialogBlockingConfirmationCoordinator
@dynamic callableDispatcher;
@synthesize request = _request;

- (instancetype)initWithRequest:(JavaScriptDialogRequest*)request {
  DCHECK(request);
  if (self = [super init]) {
    _request = request;
  }
  return self;
}

#pragma mark - BrowserCoordinator

- (void)start {
  if (self.started)
    return;
  _mediator = [[JavaScriptDialogBlockingConfirmationMediator alloc]
      initWithRequest:self.request
           dispatcher:self.callableDispatcher];
  [self.dispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(
                                   JavaScriptDialogBlockingDismissalCommands)];
  [super start];
}

- (void)stop {
  if (!self.started)
    return;
  [super stop];
  [self.dispatcher stopDispatchingToTarget:self];
}

#pragma mark - OverlayCoordinator

- (void)cancelOverlay {
  [self.request finishRequestWithSuccess:NO userInput:nil];
}

#pragma mark - JavaScriptDialogBlockingDismissalCommands

- (void)dismissJavaScriptDialogBlockingConfirmation {
  [self stop];
}

@end

@implementation JavaScriptDialogBlockingConfirmationCoordinator (
    DialogCoordinatorSubclassing)

- (UIAlertControllerStyle)alertStyle {
  // Use action sheets for compact widths and alerts for regular widths.
  UIWindow* keyWindow = [UIApplication sharedApplication].keyWindow;
  BOOL compactWidth = keyWindow.traitCollection.horizontalSizeClass ==
                      UIUserInterfaceSizeClassCompact;
  return compactWidth ? UIAlertControllerStyleActionSheet
                      : UIAlertControllerStyleAlert;
}

- (DialogMediator*)mediator {
  return _mediator;
}

@end
