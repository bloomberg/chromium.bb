// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"

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

// Spacing between the image and the text labels.
const CGFloat kHorizontalSpacingBetweenImageAndLabels = 8;

// Spacing between the labels.
const CGFloat kVerticalSpacingBetweenLabels = 8;
}  // namespace

@implementation PaymentsTextItem

@synthesize text = _text;
@synthesize detailText = _detailText;
@synthesize image = _image;
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

- (void)configureCell:(PaymentsTextCell*)cell {
  [super configureCell:cell];
  cell.accessoryType = self.accessoryType;
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  cell.imageView.image = self.image;
}

@end

@interface PaymentsTextCell () {
  NSLayoutConstraint* _labelsLeadingAnchorConstraint;
  NSLayoutConstraint* _imageLeadingAnchorConstraint;
  UIStackView* _stackView;
}
@end

@implementation PaymentsTextCell

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;
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

  _stackView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  _stackView.axis = UILayoutConstraintAxisVertical;
  _stackView.layoutMarginsRelativeArrangement = YES;
  _stackView.layoutMargins = UIEdgeInsetsMake(
      kVerticalPadding, 0, kVerticalPadding, kHorizontalPadding);
  _stackView.alignment = UIStackViewAlignmentLeading;
  _stackView.spacing = kVerticalSpacingBetweenLabels;
  _stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_stackView];

  _textLabel = [[UILabel alloc] init];
  [_stackView addArrangedSubview:_textLabel];

  _detailTextLabel = [[UILabel alloc] init];
  [_stackView addArrangedSubview:_detailTextLabel];

  _imageView = [[UIImageView alloc] initWithFrame:CGRectZero];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_imageView];
}

// Set default font and text colors for labels.
- (void)setDefaultViewStyling {
  _textLabel.font = [MDCTypography body2Font];
  _textLabel.textColor = [[MDCPalette greyPalette] tint900];
  _textLabel.numberOfLines = 0;
  _textLabel.lineBreakMode = NSLineBreakByWordWrapping;

  _detailTextLabel.font = [MDCTypography body1Font];
  _detailTextLabel.textColor = [[MDCPalette greyPalette] tint900];
  _detailTextLabel.numberOfLines = 0;
  _detailTextLabel.lineBreakMode = NSLineBreakByWordWrapping;
}

// Set constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  _labelsLeadingAnchorConstraint = [_stackView.leadingAnchor
      constraintEqualToAnchor:_imageView.trailingAnchor];
  _imageLeadingAnchorConstraint = [_imageView.leadingAnchor
      constraintEqualToAnchor:contentView.leadingAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [_stackView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor],
    [_stackView.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor],
    [_imageView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    _labelsLeadingAnchorConstraint,
    _imageLeadingAnchorConstraint,
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
  if (_imageView.image) {
    preferredMaxLayoutWidth -=
        kHorizontalSpacingBetweenImageAndLabels + _imageView.image.size.width;
    _imageLeadingAnchorConstraint.constant = kHorizontalPadding;
    _labelsLeadingAnchorConstraint.constant =
        kHorizontalSpacingBetweenImageAndLabels;
  } else {
    _imageLeadingAnchorConstraint.constant = 0;
    _labelsLeadingAnchorConstraint.constant = kHorizontalPadding;
  }
  _textLabel.preferredMaxLayoutWidth = preferredMaxLayoutWidth;
  _detailTextLabel.preferredMaxLayoutWidth = preferredMaxLayoutWidth;

  // Re-layout with the new preferred width to allow the labels to adjust thier
  // height.
  [super layoutSubviews];
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
  self.imageView.image = nil;
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  NSString* accessibilityLabel = self.textLabel.text;
  if (self.detailTextLabel.text) {
    return [NSString stringWithFormat:@"%@, %@", accessibilityLabel,
                                      self.detailTextLabel.text];
  }
  return accessibilityLabel;
}

@end
