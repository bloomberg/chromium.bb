// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_navigation_item.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kImageLength = 16;
const CGFloat kCellHeight = 44;
const CGFloat kImageTextMargin = 11;
const CGFloat kMargin = 15;
const CGFloat kVerticalMargin = 8;
const CGFloat kMaxHeight = 100;
}  // namespace

@implementation PopupMenuNavigationItem

@synthesize actionIdentifier = _actionIdentifier;
@synthesize favicon = _favicon;
@synthesize title = _title;
@synthesize navigationItem = _navigationItem;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PopupMenuNavigationCell class];
  }
  return self;
}

- (void)configureCell:(PopupMenuNavigationCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setFavicon:self.favicon];
  [cell setTitle:self.title];
}

#pragma mark - PopupMenuItem

- (CGSize)cellSizeForWidth:(CGFloat)width {
  // TODO(crbug.com/828357): This should be done at the table view level.
  static PopupMenuNavigationCell* cell;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    cell = [[PopupMenuNavigationCell alloc] init];
  });

  [self configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  cell.frame = CGRectMake(0, 0, width, kMaxHeight);
  [cell setNeedsLayout];
  [cell layoutIfNeeded];
  return [cell systemLayoutSizeFittingSize:CGSizeMake(width, kMaxHeight)];
}

@end

#pragma mark - PopupMenuNavigationCell

@interface PopupMenuNavigationCell ()
@property(nonatomic, strong) UILabel* titleLabel;
@property(nonatomic, strong) UIImageView* faviconView;
@end

@implementation PopupMenuNavigationCell

@synthesize faviconView = _faviconView;
@synthesize titleLabel = _titleLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];

    _faviconView = [[UIImageView alloc] init];
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:_titleLabel];
    [self.contentView addSubview:_faviconView];

    ApplyVisualConstraintsWithMetrics(
        @[
          @"H:|-(margin)-[image(imageSize)]-(textImage)-[text]-(margin)-|",
          @"V:[image(imageSize)]",
          @"V:|-(verticalMargin)-[text]-(verticalMargin)-|"
        ],
        @{@"image" : _faviconView, @"text" : _titleLabel}, @{
          @"margin" : @(kMargin),
          @"imageSize" : @(kImageLength),
          @"textImage" : @(kImageTextMargin),
          @"verticalMargin" : @(kVerticalMargin),
        });

    [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kCellHeight]
        .active = YES;
    [_faviconView.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor]
        .active = YES;
  }
  return self;
}

- (void)setTitle:(NSString*)title {
  self.titleLabel.text = title;
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
  [self setFavicon:nil];
}

@end
