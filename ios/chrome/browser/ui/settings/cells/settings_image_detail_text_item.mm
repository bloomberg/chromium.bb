// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"

#include "base/logging.h"
#include "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SettingsImageDetailTextCell ()

@property(nonatomic, weak, readonly) NSLayoutConstraint* imageWidthConstraint;
@property(nonatomic, weak, readonly) NSLayoutConstraint* imageHeightConstraint;

@end

#pragma mark - SettingsImageDetailTextItem

@implementation SettingsImageDetailTextItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsImageDetailTextCell class];
  }
  return self;
}

- (void)configureCell:(SettingsImageDetailTextCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  DCHECK(self.image);
  cell.imageView.image = self.image;
  cell.imageWidthConstraint.constant = self.image.size.width;
  cell.imageHeightConstraint.constant = self.image.size.height;
}

@end

#pragma mark - SettingsImageDetailTextCell

@implementation SettingsImageDetailTextCell

@synthesize imageView = _imageView;
@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    [self addSubviews];
    [self setViewConstraints];
  }
  return self;
}

// Creates and adds subviews.
- (void)addSubviews {
  UIView* contentView = self.contentView;

  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_imageView];

  _textLabel = [[UILabel alloc] init];
  _textLabel.numberOfLines = 0;
  _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _textLabel.adjustsFontForContentSizeCategory = YES;
  _textLabel.textColor = UIColor.blackColor;

  _detailTextLabel = [[UILabel alloc] init];
  _detailTextLabel.numberOfLines = 0;
  _detailTextLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _detailTextLabel.adjustsFontForContentSizeCategory = YES;
  _detailTextLabel.textColor =
      UIColorFromRGB(kTableViewSecondaryLabelLightGrayTextColor);
}

// Sets constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  UIStackView* textStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _textLabel, _detailTextLabel ]];
  textStackView.axis = UILayoutConstraintAxisVertical;
  textStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:textStackView];

  _imageWidthConstraint = [_imageView.widthAnchor constraintEqualToConstant:0];
  _imageHeightConstraint =
      [_imageView.heightAnchor constraintEqualToConstant:0];

  [NSLayoutConstraint activateConstraints:@[
    // Horizontal contraints for |_imageView| and |textStackView|.
    _imageWidthConstraint,
    [_imageView.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing],
    [textStackView.leadingAnchor
        constraintEqualToAnchor:_imageView.trailingAnchor
                       constant:kTableViewHorizontalSpacing],
    [contentView.trailingAnchor
        constraintEqualToAnchor:textStackView.trailingAnchor
                       constant:kTableViewHorizontalSpacing],
    // Vertical contraints for |_imageView| and |textStackView|.
    _imageHeightConstraint,
    [_imageView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [_imageView.topAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                    constant:kTableViewLargeVerticalSpacing],
    [contentView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:_imageView.bottomAnchor
                                    constant:kTableViewLargeVerticalSpacing],
    [textStackView.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
    [textStackView.topAnchor
        constraintGreaterThanOrEqualToAnchor:contentView.topAnchor
                                    constant:kTableViewLargeVerticalSpacing],
    [contentView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:textStackView.bottomAnchor
                                    constant:kTableViewLargeVerticalSpacing],
  ]];
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  if (self.detailTextLabel.text) {
    return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                      self.detailTextLabel.text];
  }
  return self.textLabel.text;
}

@end
