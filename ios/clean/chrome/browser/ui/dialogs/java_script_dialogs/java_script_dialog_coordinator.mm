// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_coordinator.h"

#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/commands/java_script_dialog_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_mediator.h"
#import "ios/clean/chrome/browser/ui/dialogs/java_script_dialogs/java_script_dialog_request.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface JavaScriptDialogCoordinator ()<JavaScriptDialogDismissalCommands> {
  // Backing object for property of same name (from Subclassing category).
  DialogMediator* _mediator;
}

// The dispatcher to use for JavaScriptdialogDismissalCommands.
@property(nonatomic, readonly) id<JavaScriptDialogDismissalCommands>
    dismissalDispatcher;
// The request for this dialog.
@property(nonatomic, strong) JavaScriptDialogRequest* request;

@end

@implementation JavaScriptDialogCoordinator
@synthesize request = _request;

- (instancetype)initWithRequest:(JavaScriptDialogRequest*)request {
  DCHECK(request);
  if ((self = [super init])) {
    _request = request;
  }
  return self;
}

#pragma mark - Accessors

- (id<JavaScriptDialogDismissalCommands>)dismissalDispatcher {
  return static_cast<id<JavaScriptDialogDismissalCommands>>(
      self.browser->dispatcher());
}

#pragma mark - BrowserCoordinator

- (void)start {
  _mediator = [[JavaScriptDialogMediator alloc]
      initWithRequest:self.request
           dispatcher:self.dismissalDispatcher];
  [self.browser->dispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(JavaScriptDialogDismissalCommands)];
  [super start];
}

- (void)stop {
  if (self.started)
    [self.browser->dispatcher() stopDispatchingToTarget:self];
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

@end

@implementation JavaScriptDialogCoordinator (DialogCoordinatorSubclassing)

- (DialogMediator*)mediator {
  return _mediator;
}

@end
