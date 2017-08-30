// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/context_menu/context_menu_dialog_coordinator.h"

#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/commands/context_menu_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/context_menu/context_menu_dialog_mediator.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator+subclassing.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContextMenuDialogCoordinator ()<ContextMenuDismissalCommands> {
  // Backing object for property of same name (from Subclassing category).
  ContextMenuDialogMediator* _mediator;
}

// The dispatcher to use for ContextMenuDialogMediators.
@property(nonatomic, readonly)
    id<ContextMenuCommands, ContextMenuDismissalCommands>
        mediatorDispatcher;
// The request for this dialog.
@property(nonatomic, strong) ContextMenuDialogRequest* request;

@end

@implementation ContextMenuDialogCoordinator
@synthesize request = _request;

- (instancetype)initWithRequest:(ContextMenuDialogRequest*)request {
  DCHECK(request);
  if ((self = [super init])) {
    _request = request;
  }
  return self;
}

#pragma mark - Accessors

- (id<ContextMenuCommands, ContextMenuDismissalCommands>)mediatorDispatcher {
  return static_cast<id<ContextMenuCommands, ContextMenuDismissalCommands>>(
      self.browser->dispatcher());
}

#pragma mark - BrowserCoordinator

- (void)start {
  if (self.started)
    return;
  _mediator = [[ContextMenuDialogMediator alloc] initWithRequest:self.request];
  _mediator.dispatcher = self.mediatorDispatcher;
  [self.browser->dispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(ContextMenuDismissalCommands)];
  [super start];
}

- (void)stop {
  if (!self.started)
    return;
  [self.browser->dispatcher() stopDispatchingToTarget:self];
  [super stop];
}

#pragma mark - ContextMenuDismisalCommands

- (void)dismissContextMenu {
  [self stop];
}

@end

@implementation ContextMenuDialogCoordinator (DialogCoordinatorSubclassing)

- (UIAlertControllerStyle)alertStyle {
  return UIAlertControllerStyleActionSheet;
}

- (DialogMediator*)mediator {
  return _mediator;
}

@end
