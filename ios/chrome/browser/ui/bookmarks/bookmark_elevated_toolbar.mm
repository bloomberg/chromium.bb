// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_elevated_toolbar.h"

#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MDCButton.h"
#import "ios/third_party/material_components_ios/src/components/ShadowElevations/src/MaterialShadowElevations.h"
#import "ios/third_party/material_components_ios/src/components/ShadowLayer/src/MaterialShadowLayer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kButtonHeight = 48;
}

@interface BookmarksElevatedToolbar ()

@property(nonatomic, readonly) MDCShadowLayer* shadowLayer;
@property(nonatomic, strong) MDCButton* currentButton;

@end

@implementation BookmarksElevatedToolbar

@synthesize currentButton = _currentButton;
@synthesize shadowLayer = _shadowLayer;

- (instancetype)init {
  self = [super init];
  if (self) {
    [self.layer addSublayer:[[MDCShadowLayer alloc] init]];
    self.shadowElevation = MDCShadowElevationSearchBarResting;
    self.backgroundColor = [UIColor whiteColor];
  }
  return self;
}

- (void)setButton:(MDCButton*)button {
  [self.currentButton removeFromSuperview];
  self.currentButton = button;
  if (!button)
    return;

  [self addSubview:button];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  id<LayoutGuideProvider> safeAreaLayoutGuide =
      SafeAreaLayoutGuideForView(self);
  [NSLayoutConstraint activateConstraints:@[
    [button.leadingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor],
    [button.topAnchor constraintEqualToAnchor:safeAreaLayoutGuide.topAnchor],
    [button.bottomAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.bottomAnchor],
    [button.heightAnchor constraintEqualToConstant:kButtonHeight],
  ]];
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
