// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/import_data_multiline_detail_item.h"

#import "ios/chrome/browser/ui/settings/cells/settings_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ImportDataMultilineDetailItem

@synthesize accessoryType = _accessoryType;
@synthesize text = _text;
@synthesize detailText = _detailText;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ImportDataMultilineDetailCell class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(ImportDataMultilineDetailCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.accessoryType = self.accessoryType;
  cell.textLabel.text = self.text;
  cell.detailTextLabel.text = self.detailText;
}

@end

@implementation ImportDataMultilineDetailCell

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = UIColor.blackColor;
    [contentView addSubview:_textLabel];

    _detailTextLabel = [[UILabel alloc] init];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailTextLabel.numberOfLines = 0;
    _detailTextLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    _detailTextLabel.adjustsFontForContentSizeCategory = YES;
    _detailTextLabel.textColor = UIColorFromRGB(kSettingsCellsDetailTextColor);
    [contentView addSubview:_detailTextLabel];

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_detailTextLabel.leadingAnchor
          constraintEqualToAnchor:_textLabel.leadingAnchor],
      [_detailTextLabel.trailingAnchor
          constraintEqualToAnchor:_textLabel.trailingAnchor],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.topAnchor],
    ]];
    AddOptionalVerticalPadding(contentView, _textLabel, _detailTextLabel,
                               kTableViewLargeVerticalSpacing);
  }
  return self;
}

#pragma mark Accessibility

- (NSString*)accessibilityLabel {
  return [NSString stringWithFormat:@"%@, %@", self.textLabel.text,
                                    self.detailTextLabel.text];
}

@end
