// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_detail_item.h"

#include <algorithm>

#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used on the leading and trailing edges of the cell and between the
// two labels.
const CGFloat kHorizontalPadding = 16;

// Minimum proportion of the available width to guarantee to the main and detail
// labels.
const CGFloat kMinTextWidthRatio = 0.75f;
const CGFloat kMinDetailTextWidthRatio = 0.25f;
}

@implementation CollectionViewDetailItem

@synthesize accessoryType = _accessoryType;
@synthesize text = _text;
@synthesize detailText = _detailText;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [CollectionViewDetailCell class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(CollectionViewDetailCell*)cell {
  [super configureCell:cell];
  cell.accessoryType = self.accessoryType;
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
}

@end

@implementation CollectionViewDetailCell {
  NSLayoutConstraint* _textLabelWidthConstraint;
  NSLayoutConstraint* _detailTextLabelWidthConstraint;
}

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.backgroundColor = [UIColor clearColor];
    [contentView addSubview:_textLabel];

    _textLabel.font =
        [[MDFRobotoFontLoader sharedInstance] mediumFontOfSize:14];
    _textLabel.textColor = [[MDCPalette greyPalette] tint900];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.backgroundColor = [UIColor clearColor];
    [contentView addSubview:_detailTextLabel];

    _detailTextLabel.font =
        [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:14];
    _detailTextLabel.textColor = [[MDCPalette greyPalette] tint500];

    // Set up the width constraints. They are activated here and updated in
    // layoutSubviews.
    _textLabelWidthConstraint =
        [_textLabel.widthAnchor constraintEqualToConstant:0];
    _detailTextLabelWidthConstraint =
        [_detailTextLabel.widthAnchor constraintEqualToConstant:0];

    [NSLayoutConstraint activateConstraints:@[
      // Fix the leading and trailing edges of the text labels.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_detailTextLabel.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],

      // Center the text label vertically and align the baselines of the two
      // text labels.
      [_textLabel.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_detailTextLabel.firstBaselineAnchor
          constraintEqualToAnchor:_textLabel.firstBaselineAnchor],

      _textLabelWidthConstraint,
      _detailTextLabelWidthConstraint,
    ]];
  }
  return self;
}

// Updates the layout constraints of the text labels and then calls the
// superclass's implementation of layoutSubviews which can then take account of
// the new constraints.
- (void)layoutSubviews {
  [super layoutSubviews];

  // Size the labels in order to determine how much width they want.
  [self.textLabel sizeToFit];
  [self.detailTextLabel sizeToFit];

  // Update the width constraints.
  _textLabelWidthConstraint.constant = self.textLabelTargetWidth;
  _detailTextLabelWidthConstraint.constant = self.detailTextLabelTargetWidth;

  // Now invoke the layout.
  [super layoutSubviews];
}

- (CGFloat)textLabelTargetWidth {
  CGFloat availableWidth =
      self.contentView.bounds.size.width - (3 * kHorizontalPadding);
  CGFloat textLabelWidth = self.textLabel.frame.size.width;
  CGFloat detailTextLabelWidth = self.detailTextLabel.frame.size.width;

  if (textLabelWidth + detailTextLabelWidth <= availableWidth)
    return textLabelWidth;

  return std::max(
      availableWidth - detailTextLabelWidth,
      std::min(availableWidth * kMinTextWidthRatio, textLabelWidth));
}

- (CGFloat)detailTextLabelTargetWidth {
  CGFloat availableWidth =
      self.contentView.bounds.size.width - (3 * kHorizontalPadding);
  CGFloat textLabelWidth = self.textLabel.frame.size.width;
  CGFloat detailTextLabelWidth = self.detailTextLabel.frame.size.width;

  if (textLabelWidth + detailTextLabelWidth <= availableWidth)
    return detailTextLabelWidth;

  return std::max(availableWidth - textLabelWidth,
                  std::min(availableWidth * kMinDetailTextWidthRatio,
                           detailTextLabelWidth));
}

- (NSString*)accessibilityLabel {
  return self.textLabel.text;
}

- (NSString*)accessibilityValue {
  return self.detailTextLabel.text;
}

@end
