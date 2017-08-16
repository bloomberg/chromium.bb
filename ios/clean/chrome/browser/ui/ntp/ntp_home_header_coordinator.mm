// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_home_header_coordinator.h"

#import "ios/clean/chrome/browser/ui/ntp/ntp_home_header_mediator.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_home_header_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPHomeHeaderCoordinator ()<NTPHomeHeaderMediatorAlerter>

@property(nonatomic, strong) NTPHomeHeaderMediator* mediator;
@property(nonatomic, strong) NTPHomeHeaderViewController* viewController;

@end

@implementation NTPHomeHeaderCoordinator

@synthesize delegate = _delegate;
@synthesize commandHandler = _commandHandler;
@synthesize collectionSynchronizer = _collectionSynchronizer;
@synthesize mediator = _mediator;
@synthesize viewController = _viewController;

#pragma mark - Properties

- (id<ContentSuggestionsHeaderControlling>)headerController {
  return self.mediator;
}

- (id<ContentSuggestionsHeaderProvider>)headerProvider {
  return self.mediator;
}

- (id<GoogleLandingConsumer>)consumer {
  return self.mediator;
}

- (id<ContentSuggestionsViewControllerDelegate>)collectionDelegate {
  return self.mediator;
}

#pragma mark - BrowserCoordinator

- (void)start {
  self.viewController = [[NTPHomeHeaderViewController alloc] init];

  self.mediator = [[NTPHomeHeaderMediator alloc] init];
  self.mediator.delegate = self.delegate;
  self.mediator.commandHandler = self.commandHandler;
  self.mediator.collectionSynchronizer = self.collectionSynchronizer;
  self.mediator.headerProvider = self.viewController;
  self.mediator.headerConsumer = self.viewController;
  self.mediator.alerter = self;

  [super start];
}

- (void)stop {
  [super stop];
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - NTPHomeHeaderMediatorAlerter

- (void)showAlert:(NSString*)title {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* action =
      [UIAlertAction actionWithTitle:@"Done"
                               style:UIAlertActionStyleCancel
                             handler:nil];
  [alertController addAction:action];
  [self.viewController presentViewController:alertController
                                    animated:YES
                                  completion:nil];
}

@end
