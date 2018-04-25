// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The width and height of the favicon ImageView.
const float kFaviconWidth = 28.0f;
}

@implementation TableViewURLItem

@synthesize favicon = _favicon;
@synthesize metadata = _metadata;
@synthesize title = _title;
@synthesize URL = _URL;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewURLCell class];
  }
  return self;
}

- (void)configureCell:(UITableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];

  TableViewURLCell* cell =
      base::mac::ObjCCastStrict<TableViewURLCell>(tableCell);
  [cell setFavicon:self.favicon];
  cell.titleLabel.text = self.title;
  cell.URLLabel.text = self.URL;
  cell.metadataLabel.text = self.metadata;
  cell.metadataLabel.hidden = ([self.metadata length] == 0);

  cell.faviconView.backgroundColor = styler.tableViewBackgroundColor;
  cell.titleLabel.backgroundColor = styler.tableViewBackgroundColor;
  cell.URLLabel.backgroundColor = styler.tableViewBackgroundColor;
  cell.metadataLabel.backgroundColor = styler.tableViewBackgroundColor;
}

@end

@implementation TableViewURLCell
@synthesize faviconView = _faviconView;
@synthesize metadataLabel = _metadataLabel;
@synthesize titleLabel = _titleLabel;
@synthesize URLLabel = _URLLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _faviconView = [[UIImageView alloc] init];
    _titleLabel = [[UILabel alloc] init];
    _URLLabel = [[UILabel alloc] init];
    _metadataLabel = [[UILabel alloc] init];

    // The favicon image is smaller than its UIImageView's bounds, so center
    // it.
    _faviconView.contentMode = UIViewContentModeCenter;

    // Set font sizes using dynamic type.
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _URLLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    _URLLabel.adjustsFontForContentSizeCategory = YES;
    _URLLabel.textColor = [UIColor lightGrayColor];
    _metadataLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    _metadataLabel.adjustsFontForContentSizeCategory = YES;

    // Use stack views to layout the subviews except for the favicon.
    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _URLLabel ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;
    [_metadataLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisHorizontal];
    [_metadataLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];

    // Horizontal stack view holds vertical stack view and favicon.
    UIStackView* horizontalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ verticalStack, _metadataLabel ]];
    horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    horizontalStack.spacing = kTableViewSubViewHorizontalSpacing;
    horizontalStack.distribution = UIStackViewDistributionFill;
    horizontalStack.alignment = UIStackViewAlignmentFirstBaseline;

    UIView* contentView = self.contentView;
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;
    horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_faviconView];
    [contentView addSubview:horizontalStack];

    [NSLayoutConstraint activateConstraints:@[
      // The favicon view is a fixed size, is pinned to the leading edge of the
      // content view, and is centered vertically.
      [_faviconView.heightAnchor constraintEqualToConstant:kFaviconWidth],
      [_faviconView.widthAnchor constraintEqualToConstant:kFaviconWidth],
      [_faviconView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_faviconView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      // The stack view fills the remaining space, has an intrinsic height, and
      // is centered vertically.
      [horizontalStack.leadingAnchor
          constraintEqualToAnchor:_faviconView.trailingAnchor
                         constant:kTableViewSubViewHorizontalSpacing],
      [horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [horizontalStack.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kTableViewVerticalSpacing],
      [horizontalStack.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kTableViewVerticalSpacing]
    ]];
  }
  return self;
}

- (void)setFavicon:(UIImage*)favicon {
  if (favicon) {
    self.faviconView.image = favicon;
  } else {
    self.faviconView.image = [UIImage imageNamed:@"default_favicon"];
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];
  // Reset favicon to default.
  [self setFavicon:nil];
}

@end
