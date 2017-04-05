// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/context_menu/web_context_menu_coordinator.h"

#import "ios/clean/chrome/browser/ui/context_menu/context_menu_mediator.h"
#import "ios/clean/chrome/browser/ui/context_menu/context_menu_view_controller.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/shared/chrome/browser/ui/commands/command_dispatcher.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WebContextMenuCoordinator ()
@property(nonatomic, strong) ContextMenuMediator* mediator;
@property(nonatomic, strong) ContextMenuViewController* viewController;
@end

@implementation WebContextMenuCoordinator
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

- (void)start {
  self.viewController = [[ContextMenuViewController alloc]
      initWithDispatcher:static_cast<id>(self.browser->dispatcher())];
  self.mediator =
      [[ContextMenuMediator alloc] initWithConsumer:self.viewController];
  [super start];
}

@end
