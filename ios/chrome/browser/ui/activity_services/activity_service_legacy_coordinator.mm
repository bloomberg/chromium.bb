// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_service_legacy_coordinator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/activity_services/activity_service_controller.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_password.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_snackbar.h"
#import "ios/chrome/browser/ui/activity_services/share_protocol.h"
#import "ios/chrome/browser/ui/activity_services/share_to_data.h"
#import "ios/chrome/browser/ui/activity_services/share_to_data_builder.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ActivityServiceLegacyCoordinator ()<ActivityServicePassword>
@end

@implementation ActivityServiceLegacyCoordinator

@synthesize browserState = _browserState;
@synthesize dispatcher = _dispatcher;
@synthesize tabModel = _tabModel;

@synthesize positionProvider = _positionProvider;
@synthesize presentationProvider = _presentationProvider;
@synthesize snackbarProvider = _snackbarProvider;

#pragma mark - Public methods

- (void)disconnect {
  self.browserState = nil;
  self.dispatcher = nil;
}

- (void)cancelShare {
  id<ShareProtocol> controller = [ActivityServiceController sharedInstance];
  [controller cancelShareAnimated:NO];
}

#pragma mark - Properties

- (void)setDispatcher:(CommandDispatcher*)dispatcher {
  if (dispatcher == self.dispatcher) {
    return;
  }

  if (self.dispatcher) {
    [self.dispatcher stopDispatchingToTarget:self];
  }

  [dispatcher startDispatchingToTarget:self forSelector:@selector(sharePage)];
  _dispatcher = dispatcher;
}

#pragma mark - Command handlers

- (void)sharePage {
  ShareToData* data =
      activity_services::ShareToDataForTab([self.tabModel currentTab]);
  if (!data)
    return;

  id<ShareProtocol> controller = [ActivityServiceController sharedInstance];
  if ([controller isActive])
    return;

  [controller shareWithData:data
               browserState:self.browserState
                 dispatcher:static_cast<id<BrowserCommands>>(self.dispatcher)
           passwordProvider:self
           positionProvider:self.positionProvider
       presentationProvider:self.presentationProvider
           snackbarProvider:self.snackbarProvider];
}

#pragma mark - Providers

- (PasswordController*)currentPasswordController {
  return [self.tabModel.currentTab passwordController];
}

@end
