// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_consumer.h"
#include "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

@interface ToolbarMediator ()<CRWWebStateObserver>
@end

@implementation ToolbarMediator {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
}

@synthesize consumer = _consumer;
@synthesize webState = _webState;

- (void)dealloc {
  _webStateObserver.reset();
  _webState = nullptr;
}

- (void)setWebState:(web::WebState*)webState {
  _webState = webState;
  _webStateObserver =
      base::MakeUnique<web::WebStateObserverBridge>(self.webState, self);
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  const GURL& pageURL = webState->GetVisibleURL();
  [self.consumer setCurrentPageText:base::SysUTF8ToNSString(pageURL.spec())];
}

@end
