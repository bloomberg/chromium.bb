// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_static_item.h"

#import "ios/chrome/browser/ui/badges/badge_type.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BadgeStaticItem ()

// The BadgeType of this item.
@property(nonatomic, assign) BadgeType badgeType;

@end

@implementation BadgeStaticItem
// Synthesized from protocol.
@synthesize tappable = _tappable;
// Sythesized from protocol.
@synthesize accepted = _accepted;

- (instancetype)initWithBadgeType:(BadgeType)badgeType {
  self = [super init];
  if (self) {
    _badgeType = badgeType;
    _tappable = NO;
    _accepted = NO;
  }
  return self;
}

#pragma mark - BadgeItem

- (BadgeType)badgeType {
  return _badgeType;
}

- (BOOL)isFullScreen {
  return self.badgeType == BadgeType::kBadgeTypeIncognito;
}

@end
