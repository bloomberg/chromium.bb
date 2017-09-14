// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_home_header_coordinator.h"

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_home_header_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPHomeHeaderCoordinator ()

@property(nonatomic, strong) NTPHomeHeaderViewController* viewController;

@end

@implementation NTPHomeHeaderCoordinator

@synthesize delegate = _delegate;
@synthesize commandHandler = _commandHandler;
@synthesize collectionSynchronizer = _collectionSynchronizer;
@synthesize viewController = _viewController;

#pragma mark - Properties

- (id<ContentSuggestionsHeaderControlling>)headerController {
  return self.viewController;
}

- (id<ContentSuggestionsHeaderProvider>)headerProvider {
  return self.viewController;
}

- (id<GoogleLandingConsumer>)consumer {
  return self.viewController;
}

- (void)setCollectionSynchronizer:
    (id<ContentSuggestionsCollectionSynchronizing>)collectionSynchronizer {
  _collectionSynchronizer = collectionSynchronizer;
  self.viewController.collectionSynchronizer = collectionSynchronizer;
}

#pragma mark - BrowserCoordinator

- (void)start {
  if (self.started)
    return;

  self.viewController = [[NTPHomeHeaderViewController alloc] init];
  self.viewController.delegate = self.delegate;
  self.viewController.commandHandler = self.commandHandler;

  [self.browser->broadcaster()
      addObserver:self.viewController
      forSelector:@selector(broadcastSelectedNTPPanel:)];

  [super start];
}

- (void)stop {
  [super stop];
  [self.browser->broadcaster()
      removeObserver:self.viewController
         forSelector:@selector(broadcastSelectedNTPPanel:)];
  self.viewController = nil;
}

@end
