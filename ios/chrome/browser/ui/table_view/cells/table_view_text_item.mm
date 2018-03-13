// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The inner insets of the Cell content.
const CGFloat kMargin = 16;
}

#pragma mark - TableViewTextItem

@implementation TableViewTextItem
@synthesize text = _text;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewTextCell class];
  }
  return self;
}

- (void)configureCell:(UITableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  TableViewTextCell* cell =
      base::mac::ObjCCastStrict<TableViewTextCell>(tableCell);
  cell.textLabel.text = self.text;
  cell.textLabel.backgroundColor = styler.tableViewBackgroundColor;
}

@end

#pragma mark - TableViewTextCell

@implementation TableViewTextCell
@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    // Text Label, set font sizes using dynamic type.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];

    // Container View.
    UIView* containerView = [[UIView alloc] init];
    containerView.translatesAutoresizingMaskIntoConstraints = NO;

    // Add subviews to View Hierarchy.
    [containerView addSubview:_textLabel];
    [self.contentView addSubview:containerView];

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      // Container Constraints.
      [containerView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kMargin],
      [containerView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kMargin],
      [containerView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:kMargin],
      [containerView.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                   constant:-kMargin],
      [containerView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      // Title Label Constraints.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:containerView.leadingAnchor],
      [_textLabel.topAnchor constraintEqualToAnchor:containerView.topAnchor],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:containerView.bottomAnchor],
    ]];
  }
  return self;
}

@end
