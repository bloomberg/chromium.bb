// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive/secondary_toolbar_coordinator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/toolbar/adaptive/secondary_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_visibility_configuration.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SecondaryToolbarCoordinator ()

// The ViewController, defined as a SecondaryToolbarViewController.
@property(nonatomic, strong)
    SecondaryToolbarViewController* toolbarViewController;

@end

@implementation SecondaryToolbarCoordinator

@synthesize dispatcher = _dispatcher;
@synthesize toolbarViewController = _toolbarViewController;

#pragma mark - ChromeCoordinator

- (void)start {
  BOOL isIncognito = self.browserState->IsOffTheRecord();

  ToolbarStyle style = isIncognito ? INCOGNITO : NORMAL;
  ToolbarButtonFactory* buttonFactory =
      [[ToolbarButtonFactory alloc] initWithStyle:style];
  buttonFactory.dispatcher = self.dispatcher;
  buttonFactory.dispatcher = self.dispatcher;
  buttonFactory.visibilityConfiguration =
      [[ToolbarButtonVisibilityConfiguration alloc] initWithType:PRIMARY];

  self.toolbarViewController = [[SecondaryToolbarViewController alloc]
      initWithButtonFactory:buttonFactory];
}

#pragma mark - Property accessors

- (UIViewController*)viewController {
  return self.toolbarViewController;
}

@end
