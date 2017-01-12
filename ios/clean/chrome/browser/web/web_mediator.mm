// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/web/web_mediator+internal.h"
#import "ios/clean/chrome/browser/web/web_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WebMediator ()
@property(nonatomic, readwrite, assign) web::WebState* webState;
@property(nonatomic, readwrite, strong) NSMutableDictionary* injectors;
@end

@implementation WebMediator
@synthesize webState = _webState;
@synthesize injectors = _injectors;

- (instancetype)initWithWebState:(web::WebState*)webState {
  if (self = [super init]) {
    _webState = webState;
    _injectors = [[NSMutableDictionary alloc] init];
  }
  return self;
}

@end
