// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/payments/cells/accessibility_util.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
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

// Spacing between the images and the text labels.
const CGFloat kHorizontalSpacingBetweenImageAndLabels = 8;

// Spacing between the labels.
const CGFloat kVerticalSpacingBetweenLabels = 8;
}  // namespace

@implementation PaymentsTextItem

@synthesize text = _text;
@synthesize detailText = _detailText;
@synthesize textColor = _textColor;
@synthesize detailTextColor = _detailTextColor;
@synthesize leadingImage = _leadingImage;
@synthesize trailingImage = _trailingImage;
@synthesize accessoryType = _accessoryType;
@synthesize complete = _complete;

#pragma mark CollectionViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PaymentsTextCell class];
  }
  return self;
}

- (UIColor*)textColor {
  if (!_textColor) {
    _textColor = [[MDCPalette greyPalette] tint900];
  }
  return _textColor;
}

- (UIColor*)detailTextColor {
  if (!_detailTextColor) {
    _detailTextColor = [[MDCPalette greyPalette] tint900];
  }
  return _detailTextColor;
}

- (void)configureCell:(PaymentsTextCell*)cell {
  [super configureCell:cell];
  [cell cr_setAccessoryType:self.accessoryType];
  cell.textLabel.text = self.text;
  cell.textLabel.textColor = self.textColor;
  cell.detailTextLabel.text = self.detailText;
  cell.detailTextLabel.textColor = self.detailTextColor;
  cell.leadingImageView.image = self.leadingImage;
  cell.trailingImageView.image = self.trailingImage;
}

@end

@interface PaymentsTextCell () {
  NSLayoutConstraint* _labelsLeadingAnchorConstraint;
  NSLayoutConstraint* _leadingImageLeadingAnchorConstraint;
  NSLayoutConstraint* _trailingImageTrailingAnchorConstraint;
  NSLayoutConstraint* _labelsTrailingAnchorConstraint;
  UIStackView* _stackView;
}
@end

@implementation PaymentsTextCell

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
@synthesize leadingImageView = _leadingImageView;
@synthesize trailingImageView = _trailingImageView;

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

  _stackView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  _stackView.axis = UILayoutConstraintAxisVertical;
  _stackView.layoutMarginsRelativeArrangement = YES;
  _stackView.layoutMargins = UIEdgeInsetsMakeDirected(
      kVerticalPadding, 0, kVerticalPadding, kHorizontalPadding);
  _stackView.alignment = UIStackViewAlignmentLeading;
  _stackView.spacing = kVerticalSpacingBetweenLabels;
  _stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_stackView];

  _textLabel = [[UILabel alloc] init];
  [_stackView addArrangedSubview:_textLabel];

  _detailTextLabel = [[UILabel alloc] init];
  [_stackView addArrangedSubview:_detailTextLabel];

  _leadingImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
  _leadingImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_leadingImageView];

  _trailingImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
  _trailingImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_trailingImageView];
}

// Set default font and text colors for labels.
- (void)setDefaultViewStyling {
  _textLabel.font = [MDCTypography body2Font];
  _textLabel.numberOfLines = 0;
  _textLabel.lineBreakMode = NSLineBreakByWordWrapping;

  _detailTextLabel.font = [MDCTypography body1Font];
  _detailTextLabel.numberOfLines = 0;
  _detailTextLabel.lineBreakMode = NSLineBreakByWordWrapping;
}

// Set constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  _labelsLeadingAnchorConstraint = [_stackView.leadingAnchor
      constraintEqualToAnchor:_leadingImageView.trailingAnchor];
  _leadingImageLeadingAnchorConstraint = [_leadingImageView.leadingAnchor
      constraintEqualToAnchor:contentView.leadingAnchor];
  _labelsTrailingAnchorConstraint = [_stackView.trailingAnchor
      constraintLessThanOrEqualToAnchor:_trailingImageView.leadingAnchor];
  _trailingImageTrailingAnchorConstraint = [_trailingImageView.trailingAnchor
      constraintEqualToAnchor:contentView.trailingAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [_stackView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor],
    [_stackView.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor],
    [_leadingImageView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [_trailingImageView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    _trailingImageTrailingAnchorConstraint, _labelsTrailingAnchorConstraint,
    _labelsLeadingAnchorConstraint, _leadingImageLeadingAnchorConstraint
  ]];
}

#pragma mark - UIView

// Implement -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  _textLabel.hidden = !_textLabel.text;
  _detailTextLabel.hidden = !_detailTextLabel.text;

  [super layoutSubviews];

  // Adjust the text labels' preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.frame);
  CGFloat preferredMaxLayoutWidth = parentWidth - (2 * kHorizontalPadding);
  if (_leadingImageView.image) {
    preferredMaxLayoutWidth -= kHorizontalSpacingBetweenImageAndLabels +
                               _leadingImageView.image.size.width;
    _leadingImageLeadingAnchorConstraint.constant = kHorizontalPadding;
    _labelsLeadingAnchorConstraint.constant =
        kHorizontalSpacingBetweenImageAndLabels;
  } else {
    _leadingImageLeadingAnchorConstraint.constant = 0;
    _labelsLeadingAnchorConstraint.constant = kHorizontalPadding;
  }
  if (_trailingImageView.image) {
    preferredMaxLayoutWidth -= kHorizontalSpacingBetweenImageAndLabels +
                               _trailingImageView.image.size.width;
    _trailingImageTrailingAnchorConstraint.constant = -kHorizontalPadding;
    _labelsTrailingAnchorConstraint.constant =
        -kHorizontalSpacingBetweenImageAndLabels;
  } else {
    _trailingImageTrailingAnchorConstraint.constant = 0;
    _labelsTrailingAnchorConstraint.constant = -kHorizontalPadding;
  }
  _textLabel.preferredMaxLayoutWidth = preferredMaxLayoutWidth;
  _detailTextLabel.preferredMaxLayoutWidth = preferredMaxLayoutWidth;

  // Re-layout with the new preferred width to allow the labels to adjust their
  // height.
  [super layoutSubviews];
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  self.leadingImageView.image = nil;
  self.trailingImageView.image = nil;
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  AccessibilityLabelBuilder* builder = [[AccessibilityLabelBuilder alloc] init];
  [builder appendItem:self.textLabel.text];
  [builder appendItem:self.detailTextLabel.text];
  return [builder buildAccessibilityLabel];
}

@end
