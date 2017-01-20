// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/web/web_mediator+internal.h"
#import "ios/clean/chrome/browser/web/web_mediator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WebMediator () {
  std::unique_ptr<web::WebState> _webState;
}

@property(nonatomic, readwrite, strong) NSMutableDictionary* injectors;
@end

@implementation WebMediator
@synthesize injectors = _injectors;

+ (instancetype)webMediatorForBrowserState:
    (ios::ChromeBrowserState*)browserState {
  web::WebState::CreateParams webStateCreateParams(browserState);
  std::unique_ptr<web::WebState> webState =
      web::WebState::Create(webStateCreateParams);
  webState->SetWebUsageEnabled(true);
  return [[self alloc] initWithWebState:std::move(webState)];
}

- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState {
  if (self = [super init]) {
    _webState = std::move(webState);
    _injectors = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (instancetype)init {
  return [self initWithWebState:nullptr];
}

#pragma mark - Property Implementation

- (web::WebState*)webState {
  return _webState.get();
}

@end
