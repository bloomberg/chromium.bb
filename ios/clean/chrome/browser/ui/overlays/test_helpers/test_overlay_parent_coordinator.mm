// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/overlays/test_helpers/test_overlay_parent_coordinator.h"

#import <UIKit/UIKit.h>

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestOverlayParentCoordinator ()
// The parent UIViewController.
@property(nonatomic, readonly) UIViewController* viewController;
@end

@implementation TestOverlayParentCoordinator
@synthesize viewController = _viewController;

- (instancetype)init {
  if ((self = [super init])) {
    _viewController = [[UIViewController alloc] init];
  }
  return self;
}

- (OverlayCoordinator*)presentedOverlay {
  NSUInteger childCount = self.children.count;
  DCHECK_LE(childCount, 1U);
  return childCount == 1U ? base::mac::ObjCCastStrict<OverlayCoordinator>(
                                [self.children anyObject])
                          : nil;
}

@end
