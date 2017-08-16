// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/find_in_page/find_in_page_coordinator.h"

#include <memory>

#include "base/logging.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/clean/chrome/browser/ui/commands/find_in_page_visibility_commands.h"
#import "ios/clean/chrome/browser/ui/find_in_page/find_in_page_mediator.h"
#import "ios/clean/chrome/browser/ui/find_in_page/find_in_page_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FindInPageCoordinator ()<FindInPageVisibilityCommands,
                                    FindInPageConsumerProvider>

@property(nonatomic, strong) FindInPageMediator* mediator;

@property(nonatomic, strong) FindInPageViewController* viewController;

@end

@implementation FindInPageCoordinator

@synthesize mediator = _mediator;
@synthesize viewController = _viewController;

#pragma mark - Coordinator lifecycle management

- (void)wasAddedToParentCoordinator:(BrowserCoordinator*)parent {
  DCHECK(self.browser);

  // Register command handlers with the dispatcher.
  CommandDispatcher* dispatcher = self.browser->dispatcher();
  [dispatcher startDispatchingToTarget:self
                           forSelector:@selector(showFindInPage)];
  [dispatcher startDispatchingToTarget:self
                           forSelector:@selector(hideFindInPage)];

  self.mediator = [[FindInPageMediator alloc]
      initWithWebStateList:(&self.browser->web_state_list())provider:self
                dispatcher:static_cast<id>(dispatcher)];
  [dispatcher startDispatchingToTarget:self.mediator
                           forSelector:@selector(findStringInPage:)];
  [dispatcher startDispatchingToTarget:self.mediator
                           forSelector:@selector(findNextInPage)];
  [dispatcher startDispatchingToTarget:self.mediator
                           forSelector:@selector(findPreviousInPage)];
}

- (void)willBeRemovedFromParentCoordinator {
  CommandDispatcher* dispatcher = self.browser->dispatcher();
  [dispatcher stopDispatchingToTarget:self];
  [dispatcher stopDispatchingToTarget:self.mediator];
  self.mediator = nil;
}

- (void)start {
  self.viewController = [[FindInPageViewController alloc] init];
  self.viewController.dispatcher = static_cast<id>(self.browser->dispatcher());
  [super start];
}

- (void)stop {
  [super stop];
  self.viewController = nil;
}

#pragma mark - Consumer provider

- (id<FindInPageConsumer>)consumer {
  if (!self.started) {
    [self start];
  }
  DCHECK(self.viewController);
  return self.viewController;
}

#pragma mark - Command handlers

- (void)showFindInPage {
  if (!self.started)
    [self start];
}

- (void)hideFindInPage {
  [self.mediator stopFinding];
  if (self.started)
    [self stop];
}

@end
