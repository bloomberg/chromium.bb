// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_mediator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_shared_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarkHomeMediator ()

@property(nonatomic, strong) BookmarkHomeSharedState* sharedState;

@end

@implementation BookmarkHomeMediator
@synthesize consumer = _consumer;
@synthesize sharedState = _sharedState;

- (instancetype)initWithSharedState:(BookmarkHomeSharedState*)sharedState {
  if ((self = [super init])) {
    _sharedState = sharedState;
  }
  return self;
}

- (void)startMediating {
  DCHECK(self.consumer);
  DCHECK(self.sharedState);
}

- (void)disconnect {
  self.consumer = nil;
  self.sharedState = nil;
}

@end
