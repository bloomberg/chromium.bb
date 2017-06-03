// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/context_menu/web_context_menu_coordinator.h"

#include "base/logging.h"
#import "ios/clean/chrome/browser/ui/commands/context_menu_commands.h"
#import "ios/clean/chrome/browser/ui/context_menu/context_menu_context_impl.h"
#import "ios/clean/chrome/browser/ui/context_menu/context_menu_mediator.h"
#import "ios/clean/chrome/browser/ui/context_menu/context_menu_view_controller.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/shared/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator+internal.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WebContextMenuCoordinator ()<ContextMenuCommands>
// The menu context.
@property(nonatomic, strong, readonly) ContextMenuContextImpl* context;
// The view controller that displayes the context menu UI.
@property(nonatomic, strong) ContextMenuViewController* viewController;

@end

@implementation WebContextMenuCoordinator
@synthesize context = _context;
@synthesize viewController = _viewController;

- (instancetype)initWithContext:(ContextMenuContextImpl*)context {
  if ((self = [super init])) {
    DCHECK(context);
    _context = context;
  }
  return self;
}

#pragma mark - BrowserCoordinator

- (void)start {
  id<ContextMenuCommands> contextMenuDispatcher =
      static_cast<id<ContextMenuCommands>>(self.browser->dispatcher());
  self.viewController = [[ContextMenuViewController alloc]
      initWithDispatcher:contextMenuDispatcher];
  [ContextMenuMediator updateConsumer:self.viewController
                          withContext:self.context];
  [self.browser->dispatcher()
      startDispatchingToTarget:self
                   forSelector:@selector(hideContextMenu:)];
  [super start];
}

- (void)stop {
  [self.browser->dispatcher() stopDispatchingToTarget:self];
  [super stop];
  [self.parentCoordinator removeChildCoordinator:self];
}

#pragma mark - ContextMenuCommands

- (void)hideContextMenu:(ContextMenuContext*)context {
  DCHECK_EQ(self.context, context);
  [self stop];
}

@end
