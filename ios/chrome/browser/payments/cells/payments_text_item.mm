// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/cells/payments_text_item.h"

#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding of the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Padding of the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Spacing between the image and the text label.
const CGFloat kHorizontalSpacingBetweenImageAndLabel = 8;
}  // namespace

@implementation PaymentsTextItem

@synthesize text = _text;
@synthesize image = _image;

#pragma mark CollectionViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PaymentsTextCell class];
  }
  return self;
}

- (void)configureCell:(PaymentsTextCell*)cell {
  [super configureCell:cell];
  cell.textLabel.text = self.text;
  cell.imageView.image = self.image;
}

@end

@interface PaymentsTextCell () {
  NSLayoutConstraint* _textLeadingAnchorConstraint;
  NSLayoutConstraint* _imageLeadingAnchorConstraint;
}
@end

@implementation PaymentsTextCell

@synthesize textLabel = _textLabel;
@synthesize imageView = _imageView;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    [self addSubviews];
    [self setDefaultViewStyling];
    [self setViewConstraints];
  }
  return self;
}

// Create and add subviews.
- (void)addSubviews {
  UIView* contentView = self.contentView;
  contentView.clipsToBounds = YES;

  _textLabel = [[UILabel alloc] init];
  _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_textLabel];

  _imageView = [[UIImageView alloc] initWithFrame:CGRectZero];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_imageView];
}

// Set default font and text colors for labels.
- (void)setDefaultViewStyling {
  _textLabel.font = [MDCTypography body1Font];
  _textLabel.textColor = [[MDCPalette greyPalette] tint900];
  _textLabel.numberOfLines = 0;
  _textLabel.lineBreakMode = NSLineBreakByWordWrapping;
}

// Set constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  _textLeadingAnchorConstraint = [_textLabel.leadingAnchor
      constraintEqualToAnchor:_imageView.trailingAnchor];
  _imageLeadingAnchorConstraint = [_imageView.leadingAnchor
      constraintEqualToAnchor:contentView.leadingAnchor
                     constant:kHorizontalPadding];

  [NSLayoutConstraint activateConstraints:@[
    [_textLabel.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                         constant:kVerticalPadding],
    [_textLabel.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor
                                            constant:-kVerticalPadding],
    [_imageView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    _textLeadingAnchorConstraint,
    _imageLeadingAnchorConstraint,
  ]];
}

#pragma mark - UIView

// Implement -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.frame);
  if (_imageView.image) {
    _textLabel.preferredMaxLayoutWidth =
        parentWidth - (2 * kHorizontalPadding) -
        kHorizontalSpacingBetweenImageAndLabel - _imageView.image.size.width;
    _textLeadingAnchorConstraint.constant =
        kHorizontalSpacingBetweenImageAndLabel;
  } else {
    _textLabel.preferredMaxLayoutWidth = parentWidth - (2 * kHorizontalPadding);
    _textLeadingAnchorConstraint.constant = 0;
  }

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.imageView.image = nil;
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  return [NSString stringWithFormat:@"%@", self.textLabel.text];
}

@end
