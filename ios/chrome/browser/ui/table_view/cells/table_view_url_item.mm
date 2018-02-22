// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The horizontal spacing between text labels.
const CGFloat kHorizontalSpacing = 8.0;

// THe vertical spacing between text labels.
const CGFloat kVerticalSpacing = 2.0;

// The display size of the favicon view.
const CGFloat kFaviconViewSize = 56.0;
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

- (void)configureCell:(UITableViewCell*)tableCell {
  [super configureCell:tableCell];

  TableViewURLCell* cell =
      base::mac::ObjCCastStrict<TableViewURLCell>(tableCell);
  cell.faviconView.image = self.favicon;
  cell.titleLabel.text = self.title;
  cell.URLLabel.text = self.URL;
  cell.metadataLabel.text = self.metadata;
  cell.metadataLabel.hidden = ([self.metadata length] == 0);
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
    _metadataLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    _metadataLabel.adjustsFontForContentSizeCategory = YES;

    // Use stack views to layout the subviews except for the favicon.
    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _URLLabel ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;

    verticalStack.spacing = kVerticalSpacing;
    [_metadataLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisHorizontal];
    [_metadataLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];

    UIStackView* horizontalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ verticalStack, _metadataLabel ]];
    horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    horizontalStack.spacing = kHorizontalSpacing;
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
      [_faviconView.heightAnchor constraintEqualToConstant:kFaviconViewSize],
      [_faviconView.widthAnchor constraintEqualToConstant:kFaviconViewSize],
      [_faviconView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [_faviconView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      // The stack view fills the remaining space, has an intrinsic height, and
      // is centered vertically.
      [horizontalStack.leadingAnchor
          constraintEqualToAnchor:_faviconView.trailingAnchor],
      [horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kHorizontalSpacing],
      [horizontalStack.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      // The content view's height is set by the larger of the favicon view or
      // the stack view.  This maintains a minimum size but allows the cell to
      // grow if Dynamic Type increases the font size.
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToAnchor:_faviconView.heightAnchor],
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToAnchor:horizontalStack.heightAnchor],
    ]];
  }
  return self;
}

@end
