// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/table_view_clear_browsing_data_item.h"

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Autolayout constants.
const CGFloat kVerticalPadding = 13;
const CGFloat kImageTrailingPadding = 14;
const CGFloat kImageWidth = 30;
const CGFloat kImageHeight = 30;

}  // namespace

@implementation TableViewClearBrowsingDataItem
@synthesize checked = _checked;
@synthesize dataTypeMask = _dataTypeMask;
@synthesize text = _text;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewClearBrowsingDataCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  TableViewClearBrowsingDataCell* cell =
      base::mac::ObjCCastStrict<TableViewClearBrowsingDataCell>(tableCell);
  UIImage* image = nil;
  if ([self.imageName length]) {
    image = [UIImage imageNamed:self.imageName];
  }
  [cell setImage:image];
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
  cell.optionalTextLabel.text = self.optionalText;
  cell.accessoryType = self.checked ? UITableViewCellAccessoryCheckmark
                                    : UITableViewCellAccessoryNone;
}

@end

#pragma mark - TableViewClearBrowsingDataCell

@interface TableViewClearBrowsingDataCell ()

@property(nonatomic, strong) UIImageView* imageView;
@property(nonatomic, strong) NSLayoutConstraint* imageHiddenConstraint;
@property(nonatomic, strong) NSLayoutConstraint* imageVisibleConstraint;
@property(nonatomic, copy) NSArray<NSLayoutConstraint*>* standardConstraints;
@property(nonatomic, copy)
    NSArray<NSLayoutConstraint*>* accessibilityConstraints;

// Virtual label container contains |textLabel| and |detailTextLabel|.
@property(nonatomic, strong) UILayoutGuide* labelContainerGuide;

@end

@implementation TableViewClearBrowsingDataCell

@synthesize imageView = _imageView;
@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;

    _imageView = [[UIImageView alloc] init];
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_imageView];

    _textLabel = [[UILabel alloc] init];
    _textLabel.numberOfLines = 0;
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    [self.contentView addSubview:_textLabel];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.numberOfLines = 0;
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    [self.contentView addSubview:_detailTextLabel];

    _optionalTextLabel = [[UILabel alloc] init];
    _optionalTextLabel.numberOfLines = 0;
    _optionalTextLabel.textColor =
        UIColorFromRGB(kTableViewSecondaryLabelLightGrayTextColor);
    _optionalTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _optionalTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _optionalTextLabel.adjustsFontForContentSizeCategory = YES;
    [self.contentView addSubview:_optionalTextLabel];

    _labelContainerGuide = [[UILayoutGuide alloc] init];
    [self.contentView addLayoutGuide:_labelContainerGuide];

    _imageHiddenConstraint = [_labelContainerGuide.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing];
    _imageVisibleConstraint = [_labelContainerGuide.leadingAnchor
        constraintEqualToAnchor:_imageView.trailingAnchor
                       constant:kImageTrailingPadding];
    _standardConstraints = @[
      [_imageView.centerYAnchor
          constraintEqualToAnchor:_labelContainerGuide.centerYAnchor],
    ];
    _accessibilityConstraints = @[
      [_imageView.centerYAnchor
          constraintEqualToAnchor:_textLabel.centerYAnchor],
    ];

    [NSLayoutConstraint activateConstraints:@[
      // |imageView| constraints.
      _imageHiddenConstraint,
      [_imageView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_imageView.widthAnchor constraintEqualToConstant:kImageWidth],
      [_imageView.heightAnchor constraintEqualToConstant:kImageHeight],

      // Labels x axis constraints.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:_labelContainerGuide.leadingAnchor],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:_labelContainerGuide.trailingAnchor],
      [_detailTextLabel.leadingAnchor
          constraintEqualToAnchor:_labelContainerGuide.leadingAnchor],
      [_detailTextLabel.trailingAnchor
          constraintEqualToAnchor:_labelContainerGuide.trailingAnchor],
      [_optionalTextLabel.leadingAnchor
          constraintEqualToAnchor:_labelContainerGuide.leadingAnchor],
      [_optionalTextLabel.trailingAnchor
          constraintEqualToAnchor:_labelContainerGuide.trailingAnchor],
      [_labelContainerGuide.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],

      // Labels y axis constraints.
      [_labelContainerGuide.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kVerticalPadding],
      [_textLabel.topAnchor
          constraintEqualToAnchor:_labelContainerGuide.topAnchor],
      [_detailTextLabel.topAnchor
          constraintEqualToAnchor:_textLabel.bottomAnchor],
      [_labelContainerGuide.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.bottomAnchor],
      [_optionalTextLabel.topAnchor
          constraintEqualToAnchor:_labelContainerGuide.bottomAnchor],
      [_optionalTextLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kVerticalPadding],
    ]];

    [self updateConstraintsForCategoryAccessibility:
              UIContentSizeCategoryIsAccessibilityCategory(
                  self.traitCollection.preferredContentSizeCategory)];
  }
  return self;
}

- (void)setImage:(UIImage*)image {
  self.imageView.image = image;
  self.imageView.highlightedImage =
      [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  if (self.imageView.image == nil) {
    self.imageVisibleConstraint.active = NO;
    self.imageHiddenConstraint.active = YES;
  } else {
    self.imageHiddenConstraint.active = NO;
    self.imageVisibleConstraint.active = YES;
  }
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  BOOL isPreferredCategoryAccessibility =
      UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (isPreferredCategoryAccessibility !=
      UIContentSizeCategoryIsAccessibilityCategory(
          previousTraitCollection.preferredContentSizeCategory)) {
    [self updateConstraintsForCategoryAccessibility:
              isPreferredCategoryAccessibility];
  }
}

#pragma mark - Private

// Updates layout constraints according to whether the preferred content size
// category is an accessibility category.
- (void)updateConstraintsForCategoryAccessibility:
    (BOOL)isPreferredCategoryAccessibility {
  if (isPreferredCategoryAccessibility) {
    [NSLayoutConstraint deactivateConstraints:_standardConstraints];
    [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_accessibilityConstraints];
    [NSLayoutConstraint activateConstraints:_standardConstraints];
  }
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  return self.textLabel.text;
}

- (NSString*)accessibilityValue {
  NSString* value = self.detailTextLabel.text;
  if (self.optionalTextLabel.text) {
    value = [NSString
        stringWithFormat:@"%@.%@", value, self.optionalTextLabel.text];
  }
  return value;
}

@end
