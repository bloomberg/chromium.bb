// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/autofill_data_item.h"

#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kDetailTextWidthMultiplier = 0.5;
const CGFloat kCompressionResistanceAdditionalPriority = 1;
}  // namespace

@implementation AutofillDataItem

@synthesize deletable = _deletable;
@synthesize GUID = _GUID;
@synthesize text = _text;
@synthesize leadingDetailText = _leadingDetailText;
@synthesize trailingDetailText = _trailingDetailText;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [AutofillDataCell class];
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(AutofillDataCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  cell.leadingDetailTextLabel.text = self.leadingDetailText;
  cell.trailingDetailTextLabel.text = self.trailingDetailText;
}

@end

@implementation AutofillDataCell

@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    [self addSubviews];
    [self setDefaultViewStyling];
    [self setViewConstraints];
  }
  return self;
}

// Creates and adds subviews.
- (void)addSubviews {
  UIView* contentView = self.contentView;

  _textLabel = [[UILabel alloc] init];
  _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_textLabel];

  _leadingDetailTextLabel = [[UILabel alloc] init];
  _leadingDetailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [contentView addSubview:_leadingDetailTextLabel];

  _trailingDetailTextLabel = [[UILabel alloc] init];
  _trailingDetailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [_trailingDetailTextLabel
      setContentCompressionResistancePriority:
          UILayoutPriorityDefaultHigh + kCompressionResistanceAdditionalPriority
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [contentView addSubview:_trailingDetailTextLabel];
}

// Sets default font and text colors for labels.
- (void)setDefaultViewStyling {
  _textLabel.numberOfLines = 0;
  _textLabel.lineBreakMode = NSLineBreakByWordWrapping;
  _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _textLabel.adjustsFontForContentSizeCategory = YES;
  _textLabel.textColor = UIColor.blackColor;

  _leadingDetailTextLabel.numberOfLines = 0;
  _leadingDetailTextLabel.lineBreakMode = NSLineBreakByWordWrapping;
  _leadingDetailTextLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  _leadingDetailTextLabel.adjustsFontForContentSizeCategory = YES;
  _leadingDetailTextLabel.textColor =
      UIColorFromRGB(kSettingsCellsDetailTextColor);

  _trailingDetailTextLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _trailingDetailTextLabel.textColor =
      UIColorFromRGB(kSettingsCellsDetailTextColor);
}

// Sets constraints on subviews.
- (void)setViewConstraints {
  UIView* contentView = self.contentView;

  [NSLayoutConstraint activateConstraints:@[
    // Set horizontal anchors.
    [_textLabel.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing],
    [_textLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_trailingDetailTextLabel.leadingAnchor
                                 constant:-kTableViewHorizontalSpacing],
    [_leadingDetailTextLabel.leadingAnchor
        constraintEqualToAnchor:_textLabel.leadingAnchor],
    [_leadingDetailTextLabel.trailingAnchor
        constraintEqualToAnchor:_textLabel.trailingAnchor],
    [_trailingDetailTextLabel.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor
                       constant:-kTableViewHorizontalSpacing],

    // Make sure that the detail text doesn't take too much space.
    [_trailingDetailTextLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:contentView.widthAnchor
                               multiplier:kDetailTextWidthMultiplier],

    // Set vertical anchors.
    [_leadingDetailTextLabel.topAnchor
        constraintEqualToAnchor:_textLabel.bottomAnchor],
    [_trailingDetailTextLabel.centerYAnchor
        constraintEqualToAnchor:contentView.centerYAnchor],
  ]];

  AddOptionalVerticalPadding(contentView, _textLabel, _leadingDetailTextLabel,
                             kTableViewLargeVerticalSpacing);
  AddOptionalVerticalPadding(contentView, _trailingDetailTextLabel,
                             kTableViewLargeVerticalSpacing);
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.leadingDetailTextLabel.text = nil;
  self.trailingDetailTextLabel.text = nil;
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  if (self.trailingDetailTextLabel.text) {
    return [NSString stringWithFormat:@"%@, %@, %@", self.textLabel.text,
                                      self.leadingDetailTextLabel.text,
                                      self.trailingDetailTextLabel.text];
  }
  return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                    self.leadingDetailTextLabel.text];
}

@end
