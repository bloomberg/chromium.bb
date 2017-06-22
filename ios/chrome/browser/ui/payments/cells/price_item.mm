// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/cells/price_item.h"

#include <algorithm>

#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used on the leading and trailing edges of the cell and in between the
// labels.
const CGFloat kHorizontalPadding = 16;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Minimum proportion of the available width to guarantee to the labels.
const CGFloat kMinWidthRatio = 0.5f;
}

@implementation PriceItem

@synthesize accessoryType = _accessoryType;
@synthesize complete = _complete;
@synthesize item = _item;
@synthesize notification = _notification;
@synthesize price = _price;

#pragma mark CollectionViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PriceCell class];
  }
  return self;
}

- (void)configureCell:(PriceCell*)cell {
  [super configureCell:cell];
  cell.accessoryType = self.accessoryType;
  cell.itemLabel.text = self.item;
  cell.notificationLabel.text = self.notification;
  cell.priceLabel.text = self.price;
}

@end

@implementation PriceCell {
  NSLayoutConstraint* _itemLabelWidthConstraint;
  NSLayoutConstraint* _notificationLabelWidthConstraint;
  NSLayoutConstraint* _priceLabelWidthConstraint;
}

@synthesize itemLabel = _itemLabel;
@synthesize notificationLabel = _notificationLabel;
@synthesize priceLabel = _priceLabel;

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

  _itemLabel = [[UILabel alloc] init];
  _itemLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_itemLabel];

  _notificationLabel = [[UILabel alloc] init];
  _notificationLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_notificationLabel];

  _priceLabel = [[UILabel alloc] init];
  _priceLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_priceLabel];
}

// Set default font and text colors for labels.
- (void)setDefaultViewStyling {
  _itemLabel.font = [MDCTypography body2Font];
  _itemLabel.textColor = [[MDCPalette greyPalette] tint900];

  _notificationLabel.font = [MDCTypography body2Font];
  _notificationLabel.textColor = [[MDCPalette greenPalette] tint800];

  _priceLabel.font = [MDCTypography body1Font];
  _priceLabel.textColor = [[MDCPalette greyPalette] tint900];
}

// Set constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  _itemLabelWidthConstraint =
      [_itemLabel.widthAnchor constraintEqualToConstant:0];
  _notificationLabelWidthConstraint =
      [_notificationLabel.widthAnchor constraintEqualToConstant:0];
  _priceLabelWidthConstraint =
      [_priceLabel.widthAnchor constraintEqualToConstant:0];

  [NSLayoutConstraint activateConstraints:@[
    // Fix the leading and trailing edges of the labels.
    [_itemLabel.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor
                                             constant:kHorizontalPadding],
    [_notificationLabel.trailingAnchor
        constraintEqualToAnchor:_priceLabel.leadingAnchor
                       constant:-kHorizontalPadding],
    [_priceLabel.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kHorizontalPadding],

    // Set up the top and bottom constraints for |_itemLabel| and align the
    // baselines of the other two labels with that of the |_itemLabel|.
    [_itemLabel.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                         constant:kVerticalPadding],
    [_itemLabel.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor
                                            constant:-kVerticalPadding],
    [_notificationLabel.firstBaselineAnchor
        constraintEqualToAnchor:_itemLabel.firstBaselineAnchor],
    [_priceLabel.firstBaselineAnchor
        constraintEqualToAnchor:_itemLabel.firstBaselineAnchor],

    _itemLabelWidthConstraint,
    _notificationLabelWidthConstraint,
    _priceLabelWidthConstraint,
  ]];
}

#pragma mark - UIView

// Updates the width constraints of the text labels and then calls the
// superclass's implementation of layoutSubviews which will then take the new
// constraints into account.
- (void)layoutSubviews {
  [super layoutSubviews];

  // Size the labels in order to determine how much width they want.
  [self.itemLabel sizeToFit];
  [self.notificationLabel sizeToFit];
  [self.priceLabel sizeToFit];

  // Update the width constraint of the labels.
  _priceLabelWidthConstraint.constant = self.priceLabelTargetWidth;
  _itemLabelWidthConstraint.constant = self.itemLabelTargetWidth;
  _notificationLabelWidthConstraint.constant =
      self.notificationLabelTargetWidth;

  // Now invoke the layout.
  [super layoutSubviews];
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.accessoryType = MDCCollectionViewCellAccessoryNone;
  self.itemLabel.text = nil;
  self.notificationLabel.text = nil;
  self.priceLabel.text = nil;
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  return [NSString stringWithFormat:@"%@, %@, %@", self.itemLabel.text,
                                    self.notificationLabel.text,
                                    self.priceLabel.text];
}

@end

@implementation PriceCell (TestingOnly)

- (CGFloat)itemLabelTargetWidth {
  CGFloat itemLabelWidth = self.itemLabel.frame.size.width;
  CGFloat notificationLabelWidth = self.notificationLabel.frame.size.width;
  CGFloat priceLabelWidth = self.priceLabel.frame.size.width;

  CGFloat horizontalPadding = (notificationLabelWidth > 0)
                                  ? 4 * kHorizontalPadding
                                  : 3 * kHorizontalPadding;
  CGFloat availableWidth = CGRectGetWidth(self.contentView.frame) -
                           self.priceLabelTargetWidth - horizontalPadding;

  if (itemLabelWidth + notificationLabelWidth + priceLabelWidth <=
      availableWidth) {
    return itemLabelWidth;
  } else {
    return std::max(availableWidth - notificationLabelWidth,
                    std::min(availableWidth * kMinWidthRatio, itemLabelWidth));
  }
}

- (CGFloat)notificationLabelTargetWidth {
  CGFloat itemLabelWidth = self.itemLabel.frame.size.width;
  CGFloat notificationLabelWidth = self.notificationLabel.frame.size.width;
  CGFloat priceLabelWidth = self.priceLabel.frame.size.width;

  CGFloat horizontalPadding = (notificationLabelWidth > 0)
                                  ? 4 * kHorizontalPadding
                                  : 3 * kHorizontalPadding;
  CGFloat availableWidth = CGRectGetWidth(self.contentView.frame) -
                           self.priceLabelTargetWidth - horizontalPadding;

  if (itemLabelWidth + notificationLabelWidth + priceLabelWidth <=
      availableWidth) {
    return notificationLabelWidth;
  } else {
    return std::max(
        availableWidth - itemLabelWidth,
        std::min(availableWidth * kMinWidthRatio, notificationLabelWidth));
  }
}

- (CGFloat)priceLabelTargetWidth {
  CGFloat itemLabelWidth = self.itemLabel.frame.size.width;
  CGFloat notificationLabelWidth = self.notificationLabel.frame.size.width;
  CGFloat priceLabelWidth = self.priceLabel.frame.size.width;

  CGFloat horizontalPadding = (notificationLabelWidth > 0)
                                  ? 4 * kHorizontalPadding
                                  : 3 * kHorizontalPadding;
  CGFloat availableWidth =
      CGRectGetWidth(self.contentView.frame) - horizontalPadding;

  if (itemLabelWidth + notificationLabelWidth + priceLabelWidth <=
      availableWidth) {
    return priceLabelWidth;
  } else {
    return std::max(availableWidth - itemLabelWidth - notificationLabelWidth,
                    std::min(availableWidth * kMinWidthRatio, priceLabelWidth));
  }
}

@end
