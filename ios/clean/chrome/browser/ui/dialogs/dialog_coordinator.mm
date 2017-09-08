// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_mediator.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DialogCoordinator ()

// The dispatcher for DialogDismissalCommands.
@property(nonatomic, readonly) id<DialogDismissalCommands> callableDispatcher;
// The view controller used to display this dialog.
@property(nonatomic, strong) DialogViewController* viewController;

@end

@implementation DialogCoordinator
@synthesize viewController = _viewController;
@dynamic callableDispatcher;

#pragma mark - BrowserCoordinator

- (void)start {
  DCHECK(self.mediator);
  self.viewController =
      [[DialogViewController alloc] initWithStyle:self.alertStyle
                                       dispatcher:self.callableDispatcher];
  [self.mediator updateConsumer:self.viewController];
  [self.dispatcher startDispatchingToTarget:self.mediator
                                forProtocol:@protocol(DialogDismissalCommands)];
  [super start];
}

- (void)stop {
  [self.dispatcher stopDispatchingToTarget:self.mediator];
  [super stop];
}

@end

@implementation DialogCoordinator (DialogCoordinatorSubclassing)

- (UIAlertControllerStyle)alertStyle {
  return UIAlertControllerStyleAlert;
}

- (DialogMediator*)mediator {
  // Implemented by subclasses.
  NOTREACHED();
  return nil;
}

@end
