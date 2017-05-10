// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"

#import "ios/chrome/browser/ui/favicon/favicon_attributes.h"
#import "ios/chrome/browser/ui/favicon/favicon_view.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kLabelTextColor = 0.314;
const NSInteger kLabelNumLines = 2;
const CGFloat kFaviconSize = 48;
const CGFloat kSpaceFaviconTitle = 10;

// Size of a Most Visited cell.
const CGSize kCellSize = {73, 100};
}

#pragma mark - ContentSuggestionsMostVisitedItem

@implementation ContentSuggestionsMostVisitedItem

@synthesize suggestionIdentifier = _suggestionIdentifier;
@synthesize attributes = _attributes;
@synthesize title = _title;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsMostVisitedCell class];
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsMostVisitedCell*)cell {
  [super configureCell:cell];
  cell.titleLabel.text = self.title;
  cell.accessibilityLabel = self.title;
  [cell.faviconView configureWithAttributes:self.attributes];
}

@end

#pragma mark - ContentSuggestionsMostVisitedCell

@implementation ContentSuggestionsMostVisitedCell : MDCCollectionViewCell

@synthesize faviconView = _faviconView;
@synthesize titleLabel = _titleLabel;

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.textColor = [UIColor colorWithWhite:kLabelTextColor alpha:1.0];
    _titleLabel.font = [MDCTypography captionFont];
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.preferredMaxLayoutWidth = [[self class] defaultSize].width;
    _titleLabel.numberOfLines = kLabelNumLines;

    _faviconView = [[FaviconViewNew alloc] init];
    _faviconView.font = [MDCTypography headlineFont];

    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:_titleLabel];
    [self addSubview:_faviconView];

    [NSLayoutConstraint activateConstraints:@[
      [_faviconView.widthAnchor constraintEqualToConstant:kFaviconSize],
      [_faviconView.heightAnchor
          constraintEqualToAnchor:_faviconView.widthAnchor],
      [_faviconView.centerXAnchor
          constraintEqualToAnchor:_titleLabel.centerXAnchor],
    ]];

    ApplyVisualConstraintsWithMetrics(
        @[ @"V:|[favicon]-(space)-[title]", @"H:|[title]|" ],
        @{ @"favicon" : _faviconView,
           @"title" : _titleLabel },
        @{ @"space" : @(kSpaceFaviconTitle) });

    self.isAccessibilityElement = YES;
  }
  return self;
}

+ (CGSize)defaultSize {
  return kCellSize;
}

- (CGSize)intrinsicContentSize {
  return [[self class] defaultSize];
}

@end
