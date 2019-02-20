// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"

#include "base/i18n/rtl.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Vertical spacing between label and the container view of a cell.
const CGFloat kLabelCellVerticalSpacing = 11.0;
}  // namespace

@implementation TableViewImageItem

@synthesize image = _image;
@synthesize title = _title;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewImageCell class];
    _enabled = YES;
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];

  TableViewImageCell* cell =
      base::mac::ObjCCastStrict<TableViewImageCell>(tableCell);
  if (self.image) {
    cell.imageView.hidden = NO;
    cell.imageView.image = self.image;
  } else {
    // No image. Hide imageView.
    cell.imageView.hidden = YES;
  }

  cell.titleLabel.text = self.title;
  UIColor* cellBackgroundColor = styler.cellBackgroundColor
                                     ? styler.cellBackgroundColor
                                     : styler.tableViewBackgroundColor;
  cell.imageView.backgroundColor = cellBackgroundColor;
  cell.titleLabel.backgroundColor = cellBackgroundColor;
  if (self.textColor) {
    cell.titleLabel.textColor = self.textColor;
  } else if (styler.cellTitleColor) {
    cell.titleLabel.textColor = styler.cellTitleColor;
  }

  cell.userInteractionEnabled = self.enabled;
}

@end

@implementation TableViewImageCell

// This property overrides the one from UITableViewCell, so this @synthesize
// cannot be removed.
@synthesize imageView = _imageView;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _imageView = [[UIImageView alloc] init];
    // The favicon image is smaller than its UIImageView's bounds, so center it.
    _imageView.contentMode = UIViewContentModeCenter;
    [_imageView setContentHuggingPriority:UILayoutPriorityRequired
                                  forAxis:UILayoutConstraintAxisHorizontal];

    // Set font size using dynamic type.
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    [_titleLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];

    UIStackView* horizontalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _imageView, _titleLabel ]];
    horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
    horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    horizontalStack.spacing = kTableViewSubViewHorizontalSpacing;
    horizontalStack.distribution = UIStackViewDistributionFill;
    horizontalStack.alignment = UIStackViewAlignmentCenter;

    [self.contentView addSubview:horizontalStack];
    [NSLayoutConstraint activateConstraints:@[
      // Horizontal Stack constraints.
      [horizontalStack.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [horizontalStack.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kLabelCellVerticalSpacing],
      [horizontalStack.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kLabelCellVerticalSpacing],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.userInteractionEnabled = YES;
}

@end
