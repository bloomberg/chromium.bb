// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/cells/payment_method_item.h"

#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Vertical spacing between the labels.
const CGFloat kVerticalSpacingBetweenLabels = 8;

// Padding used on the leading and trailing edges of the cell and between the
// favicon and labels.
const CGFloat kHorizontalPadding = 16;
}

@implementation PaymentMethodItem

@synthesize methodID = _methodID;
@synthesize methodDetail = _methodDetail;
@synthesize methodTypeIcon = _methodTypeIcon;
@synthesize accessoryType = _accessoryType;

#pragma mark CollectionViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PaymentMethodCell class];
  }
  return self;
}

- (void)configureCell:(PaymentMethodCell*)cell {
  [super configureCell:cell];
  cell.methodIDLabel.text = self.methodID;
  cell.methodDetailLabel.text = self.methodDetail;
  cell.methodTypeIconView.image = self.methodTypeIcon;
  cell.accessoryType = self.accessoryType;
}

@end

@implementation PaymentMethodCell {
  NSLayoutConstraint* _iconHeightConstraint;
  NSLayoutConstraint* _iconWidthConstraint;
}

@synthesize methodIDLabel = _methodIDLabel;
@synthesize methodDetailLabel = _methodDetailLabel;
@synthesize methodTypeIconView = _methodTypeIconView;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;

    // Method ID.
    _methodIDLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _methodIDLabel.font = [MDCTypography body2Font];
    _methodIDLabel.textColor = [[MDCPalette greyPalette] tint900];
    _methodIDLabel.numberOfLines = 0;
    _methodIDLabel.backgroundColor = [UIColor clearColor];
    _methodIDLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_methodIDLabel];

    // Method detail.
    _methodDetailLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _methodDetailLabel.font = [MDCTypography body1Font];
    _methodDetailLabel.textColor = [[MDCPalette greyPalette] tint900];
    _methodDetailLabel.numberOfLines = 0;
    _methodDetailLabel.backgroundColor = [UIColor clearColor];
    _methodDetailLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_methodDetailLabel];

    // Method type icon.
    _methodTypeIconView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _methodTypeIconView.layer.borderColor =
        [UIColor colorWithWhite:0.9 alpha:1.0].CGColor;
    _methodTypeIconView.layer.borderWidth = 1.0;
    _methodTypeIconView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_methodTypeIconView];

    // Set up the icons size constraints. They are activated here and updated in
    // layoutSubviews.
    _iconHeightConstraint =
        [_methodTypeIconView.heightAnchor constraintEqualToConstant:0];
    _iconWidthConstraint =
        [_methodTypeIconView.widthAnchor constraintEqualToConstant:0];

    // Layout
    [NSLayoutConstraint activateConstraints:@[
      _iconHeightConstraint,
      _iconWidthConstraint,

      [_methodIDLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_methodDetailLabel.leadingAnchor
          constraintEqualToAnchor:_methodIDLabel.leadingAnchor],

      [_methodIDLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_methodTypeIconView.leadingAnchor
                                   constant:-kHorizontalPadding],
      [_methodDetailLabel.trailingAnchor
          constraintEqualToAnchor:_methodIDLabel.trailingAnchor],

      [_methodIDLabel.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kVerticalPadding],
      [_methodIDLabel.bottomAnchor
          constraintEqualToAnchor:_methodDetailLabel.topAnchor
                         constant:-kVerticalSpacingBetweenLabels],
      [_methodDetailLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kVerticalPadding],

      [_methodTypeIconView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [_methodTypeIconView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];
  }
  return self;
}

#pragma mark - UIView

- (void)layoutSubviews {
  // Force the accessory type to Checkmark for the duration of layout in order
  // to make the content area the same size for all items, regardless of whether
  // they're checked or not.
  MDCCollectionViewCellAccessoryType realAccessoryType = self.accessoryType;
  self.accessoryType = MDCCollectionViewCellAccessoryCheckmark;

  // Set the size constraints of the icon view to the dimensions of the image.
  _iconHeightConstraint.constant = self.methodTypeIconView.image.size.height;
  _iconWidthConstraint.constant = self.methodTypeIconView.image.size.width;

  // Implement width adjustment per instructions in documentation for
  // +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
  [super layoutSubviews];

  // Adjust labels' preferredMaxLayoutWidth when the parent's width changes, for
  // instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.frame);
  CGFloat preferredMaxLayoutWidth =
      parentWidth - (_iconWidthConstraint.constant + (3 * kHorizontalPadding));
  _methodIDLabel.preferredMaxLayoutWidth = preferredMaxLayoutWidth;
  _methodDetailLabel.preferredMaxLayoutWidth = preferredMaxLayoutWidth;

  [super layoutSubviews];

  // Restore the real accessory type at the end of the layout.
  self.accessoryType = realAccessoryType;
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.methodIDLabel.text = nil;
  self.methodDetailLabel.text = nil;
  self.methodTypeIconView.image = nil;
  self.accessoryType = MDCCollectionViewCellAccessoryNone;
}

#pragma mark - Accessibility

- (NSString*)accessibilityLabel {
  return [NSString stringWithFormat:@"%@, %@", self.methodIDLabel.text,
                                    self.methodDetailLabel.text];
}

@end
