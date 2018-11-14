// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/browser_coordinator.h"

#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view_controller_dependency_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BrowserCoordinator
@synthesize viewController = _viewController;
@synthesize applicationCommandHandler = _applicationCommandHandler;
@synthesize tabModel = _tabModel;

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK(self.browserState);
  DCHECK(!self.viewController);
  _viewController = [self createViewController];
  [super start];
}

- (void)stop {
  [_viewController browserStateDestroyed];
  [_viewController shutdown];
  _viewController = nil;
  [super stop];
}

#pragma mark - Private

// Instantiate a BrowserViewController.
- (BrowserViewController*)createViewController {
  BrowserViewControllerDependencyFactory* factory =
      [[BrowserViewControllerDependencyFactory alloc]
          initWithBrowserState:self.browserState
                  webStateList:[self.tabModel webStateList]];
  return [[BrowserViewController alloc]
                initWithTabModel:self.tabModel
                    browserState:self.browserState
               dependencyFactory:factory
      applicationCommandEndpoint:self.applicationCommandHandler];
}

@end
