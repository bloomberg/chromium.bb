// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab/tab_navigation_controller.h"

#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/clean/chrome/browser/ui/commands/navigation_commands.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabNavigationController
@synthesize dispatcher = _dispatcher;
@synthesize webState = _webState;

- (instancetype)initWithDispatcher:(CommandDispatcher*)dispatcher
                          webState:(web::WebState*)webState {
  self = [super init];
  DCHECK(webState);
  if (self) {
    _webState = webState;
    _dispatcher = dispatcher;
    [_dispatcher startDispatchingToTarget:self forSelector:@selector(goBack)];
    [_dispatcher startDispatchingToTarget:self
                              forSelector:@selector(goForward)];
    [_dispatcher startDispatchingToTarget:self
                              forSelector:@selector(reloadPage)];
    [_dispatcher startDispatchingToTarget:self
                              forSelector:@selector(stopLoadingPage)];
  }
  return self;
}

- (void)stop {
  [self.dispatcher stopDispatchingToTarget:self];
}

#pragma mark - NavigationCommands

- (void)goBack {
  self.webState->GetNavigationManager()->GoBack();
}

- (void)goForward {
  self.webState->GetNavigationManager()->GoForward();
}

- (void)stopLoadingPage {
  self.webState->Stop();
}

- (void)reloadPage {
  self.webState->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                                false /*check_for_repost*/);
}

@end
