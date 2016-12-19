// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/favicon_view.h"

#include "base/mac/objc_property_releaser.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

namespace {
// Default corner radius for the favicon image view.
const CGFloat kDefaultCornerRadius = 3;
}

@interface FaviconView () {
  // Property releaser for FaviconView.
  base::mac::ObjCPropertyReleaser _propertyReleaser_FaviconView;
}
// Size constraints for the favicon views.
@property(nonatomic, copy) NSArray* faviconSizeConstraints;
@end

@implementation FaviconView

@synthesize size = _size;
@synthesize faviconImage = _faviconImage;
@synthesize faviconFallbackLabel = _faviconFallbackLabel;
@synthesize faviconSizeConstraints = _faviconSizeConstraints;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _propertyReleaser_FaviconView.Init(self, [FaviconView class]);
    _faviconImage = [[UIImageView alloc] init];
    _faviconImage.clipsToBounds = YES;
    _faviconImage.layer.cornerRadius = kDefaultCornerRadius;
    _faviconImage.image = nil;

    _faviconFallbackLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _faviconFallbackLabel.backgroundColor = [UIColor clearColor];
    _faviconFallbackLabel.textAlignment = NSTextAlignmentCenter;
    _faviconFallbackLabel.isAccessibilityElement = NO;
    _faviconFallbackLabel.text = nil;

    [self addSubview:_faviconImage];
    [self addSubview:_faviconFallbackLabel];

    [_faviconImage setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_faviconFallbackLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
    AddSameCenterConstraints(_faviconImage, self);
    AddSameSizeConstraint(_faviconImage, self);
    AddSameCenterConstraints(_faviconFallbackLabel, self);
    AddSameSizeConstraint(_faviconFallbackLabel, self);
    _faviconSizeConstraints = [@[
      [self.widthAnchor constraintEqualToConstant:0],
      [self.heightAnchor constraintEqualToConstant:0],
    ] retain];
    [NSLayoutConstraint activateConstraints:_faviconSizeConstraints];
  }
  return self;
}

- (void)setSize:(CGFloat)size {
  _size = size;
  for (NSLayoutConstraint* constraint in self.faviconSizeConstraints) {
    constraint.constant = size;
  }
}

@end
