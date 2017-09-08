// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/http_auth_dialogs/http_auth_dialog_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/commands/http_auth_dialog_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/http_auth_dialogs/http_auth_dialog_mediator.h"
#import "ios/clean/chrome/browser/ui/dialogs/http_auth_dialogs/http_auth_dialog_request.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HTTPAuthDialogCoordinator ()<HTTPAuthDialogDismissalCommands> {
  // Backing object for property of same name (from Subclassing category).
  HTTPAuthDialogMediator* _mediator;
}

// The dispatcher to provide to the mediator.
@property(nonatomic, readonly, strong) id<HTTPAuthDialogDismissalCommands>
    callableDispatcher;
// The request used for this dialog.
@property(nonatomic, strong) HTTPAuthDialogRequest* request;

@end

@implementation HTTPAuthDialogCoordinator
@synthesize request = _request;
@dynamic callableDispatcher;

- (instancetype)initWithRequest:(HTTPAuthDialogRequest*)request {
  DCHECK(request);
  if ((self = [super init])) {
    _request = request;
  }
  return self;
}

#pragma mark - BrowserCoordinator

- (void)start {
  _mediator = [[HTTPAuthDialogMediator alloc] initWithRequest:self.request];
  _mediator.dispatcher = self.callableDispatcher;
  [self.dispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(HTTPAuthDialogDismissalCommands)];
  [super start];
}

- (void)stop {
  [self.dispatcher stopDispatchingToTarget:self];
  [super stop];
}

#pragma mark - OverlayCoordinator

- (void)cancelOverlay {
  [self.request completeAuthenticationWithUsername:nil password:nil];
}

#pragma mark - HTTPAuthDialogDismissalCommands

- (void)dismissHTTPAuthDialog {
  [self stop];
}

@end

@implementation HTTPAuthDialogCoordinator (DialogCoordinatorSubclassing)

- (DialogMediator*)mediator {
  return _mediator;
}

@end
