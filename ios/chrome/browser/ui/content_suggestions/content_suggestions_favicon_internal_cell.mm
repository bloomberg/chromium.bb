// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_favicon_internal_cell.h"

#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kFaviconImageSize = 50;
const CGFloat kFontSize = 10;
}

@implementation ContentSuggestionsFaviconInternalCell

@synthesize faviconView = _faviconView;
@synthesize titleLabel = _titleLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _faviconView = [[UIImageView alloc] init];
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.numberOfLines = 2;
    _titleLabel.font = [[MDCTypography fontLoader] lightFontOfSize:kFontSize];
    _titleLabel.textAlignment = NSTextAlignmentCenter;

    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_faviconView];
    [self.contentView addSubview:_titleLabel];

    ApplyVisualConstraints(
        @[ @"V:|-[image]-[title]-|" ],
        @{ @"image" : _faviconView,
           @"title" : _titleLabel });

    [NSLayoutConstraint activateConstraints:@[
      [_faviconView.centerXAnchor
          constraintEqualToAnchor:self.contentView.centerXAnchor],
      [_titleLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [_titleLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor],
      [_faviconView.widthAnchor constraintEqualToConstant:kFaviconImageSize],
      [_faviconView.heightAnchor
          constraintEqualToAnchor:_faviconView.widthAnchor],
    ]];
  }
  return self;
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.faviconView.image = nil;
  self.titleLabel.text = nil;
}

+ (NSString*)reuseIdentifier {
  return @"faviconInternalCell";
}

@end
