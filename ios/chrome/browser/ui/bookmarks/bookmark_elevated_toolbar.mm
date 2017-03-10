// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_elevated_toolbar.h"

#include "base/mac/objc_property_releaser.h"
#import "ios/third_party/material_components_ios/src/components/ShadowElevations/src/MaterialShadowElevations.h"
#import "ios/third_party/material_components_ios/src/components/ShadowLayer/src/MaterialShadowLayer.h"

@implementation BookmarksElevatedToolbar {
  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarksElevatedToolbar;
}

@synthesize shadowLayer = _shadowLayer;

#pragma mark - Lifecycle

- (instancetype)init {
  self = [super init];
  if (self) {
    _propertyReleaser_BookmarksElevatedToolbar.Init(
        self, [BookmarksElevatedToolbar class]);
  }
  return self;
}

#pragma mark - Properties

- (MDCShadowLayer*)shadowLayer {
  if (!_shadowLayer) {
    _shadowLayer = [[MDCShadowLayer alloc] init];
    _shadowLayer.elevation = MDCShadowElevationNone;
    [self.layer addSublayer:self.shadowLayer];
  }
  return _shadowLayer;
}

- (void)setShadowElevation:(CGFloat)shadowElevation {
  self.shadowLayer.elevation = shadowElevation;
}

- (CGFloat)shadowElevation {
  return self.shadowLayer.elevation;
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  self.shadowLayer.frame = self.layer.bounds;
}

@end
