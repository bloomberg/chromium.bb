// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/cells/shipping_address_item.h"

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

// Spacing between the labels.
const CGFloat kVerticalSpacingBetweenLabels = 8;
}  // namespace

@implementation ShippingAddressItem

@synthesize name = _name;
@synthesize address = _address;
@synthesize phoneNumber = _phoneNumber;
@synthesize accessoryType = _accessoryType;

#pragma mark CollectionViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ShippingAddressCell class];
  }
  return self;
}

- (void)configureCell:(ShippingAddressCell*)cell {
  [super configureCell:cell];
  cell.accessoryType = self.accessoryType;
  cell.nameLabel.text = self.name;
  cell.addressLabel.text = self.address;
  cell.phoneNumberLabel.text = self.phoneNumber;
}

@end

@implementation ShippingAddressCell

@synthesize nameLabel = _nameLabel;
@synthesize addressLabel = _addressLabel;
@synthesize phoneNumberLabel = _phoneNumberLabel;

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

  _nameLabel = [[UILabel alloc] init];
  _nameLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_nameLabel];

  _addressLabel = [[UILabel alloc] init];
  _addressLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_addressLabel];

  _phoneNumberLabel = [[UILabel alloc] init];
  _phoneNumberLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_phoneNumberLabel];
}

// Set default font and text colors for labels.
- (void)setDefaultViewStyling {
  _nameLabel.font = [MDCTypography body2Font];
  _nameLabel.textColor = [[MDCPalette greyPalette] tint900];
  _nameLabel.numberOfLines = 0;
  _nameLabel.lineBreakMode = NSLineBreakByWordWrapping;

  _addressLabel.font = [MDCTypography body1Font];
  _addressLabel.textColor = [[MDCPalette greyPalette] tint900];
  _addressLabel.numberOfLines = 0;
  _addressLabel.lineBreakMode = NSLineBreakByWordWrapping;

  _phoneNumberLabel.font = [MDCTypography body1Font];
  _phoneNumberLabel.textColor = [[MDCPalette greyPalette] tint900];
}

// Set constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  [NSLayoutConstraint activateConstraints:@[
    // Set leading anchors.
    [_nameLabel.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor
                                             constant:kHorizontalPadding],
    [_addressLabel.leadingAnchor
        constraintEqualToAnchor:_nameLabel.leadingAnchor],
    [_phoneNumberLabel.leadingAnchor
        constraintEqualToAnchor:_addressLabel.leadingAnchor],

    // Set vertical anchors.
    [_nameLabel.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                         constant:kVerticalPadding],
    [_addressLabel.topAnchor
        constraintEqualToAnchor:_nameLabel.bottomAnchor
                       constant:kVerticalSpacingBetweenLabels],
    [_addressLabel.bottomAnchor
        constraintEqualToAnchor:_phoneNumberLabel.topAnchor
                       constant:-kVerticalSpacingBetweenLabels],
    [_phoneNumberLabel.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor
                       constant:-kVerticalPadding],

    // Set trailing anchors.
    [_nameLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:contentView.trailingAnchor
                                 constant:-kHorizontalPadding],
    [_addressLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_nameLabel.trailingAnchor],
    [_phoneNumberLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_addressLabel.trailingAnchor],
  ]];
}

#pragma mark - UIView

// Implement -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  // When the accessory type is None, the content view of the cell (and thus)
  // the labels inside it span larger than when there is a Checkmark accessory
  // type. That means that toggling the accessory type can induce a rewrapping
  // of the texts, which is not visually pleasing. To alleviate that issue
  // always lay out the cell as if there was a Checkmark accessory type.
  //
  // Force the accessory type to Checkmark for the duration of layout.
  MDCCollectionViewCellAccessoryType realAccessoryType = self.accessoryType;
  self.accessoryType = MDCCollectionViewCellAccessoryCheckmark;

  [super layoutSubviews];

  // Adjust labels' preferredMaxLayoutWidth when the parent's width changes, for
  // instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.frame);
  CGFloat preferredMaxLayoutWidth = parentWidth - (2 * kHorizontalPadding);
  _nameLabel.preferredMaxLayoutWidth = preferredMaxLayoutWidth;
  _addressLabel.preferredMaxLayoutWidth = preferredMaxLayoutWidth;
  _phoneNumberLabel.preferredMaxLayoutWidth = preferredMaxLayoutWidth;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];

  // Restore the real accessory type at the end of the layout.
  self.accessoryType = realAccessoryType;
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.nameLabel.text = nil;
  self.addressLabel.text = nil;
  self.phoneNumberLabel.text = nil;
  self.accessoryType = MDCCollectionViewCellAccessoryNone;
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  return [NSString stringWithFormat:@"%@, %@, %@", self.nameLabel.text,
                                    self.addressLabel.text,
                                    self.phoneNumberLabel.text];
}

@end
