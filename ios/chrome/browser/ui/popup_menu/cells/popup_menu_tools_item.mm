// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_tools_item.h"

#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kImageLength = 30;
}

@implementation PopupMenuToolsItem

@synthesize actionIdentifier = _actionIdentifier;
@synthesize image = _image;
@synthesize title = _title;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PopupMenuToolsCell class];
  }
  return self;
}

- (void)configureCell:(PopupMenuToolsCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setTitleText:self.title];
  cell.imageView.image = self.image;
}

@end

#pragma mark - PopupMenuToolsCell

@interface PopupMenuToolsCell ()

// Title label for the cell.
@property(nonatomic, strong) UILabel* title;
// Image view for the cell, redefined as readwrite.
@property(nonatomic, strong, readwrite) UIImageView* imageView;

@end

@implementation PopupMenuToolsCell

@synthesize imageView = _imageView;
@synthesize title = _title;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _title = [[UILabel alloc] init];
    _title.translatesAutoresizingMaskIntoConstraints = NO;

    _imageView = [[UIImageView alloc] init];
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_imageView.widthAnchor constraintEqualToConstant:kImageLength],
      [_imageView.heightAnchor
          constraintGreaterThanOrEqualToConstant:kImageLength],
    ]];

    [self.contentView addSubview:_title];
    [self.contentView addSubview:_imageView];

    AddSameConstraintsToSides(
        self.contentView, _title,
        LayoutSides::kTop | LayoutSides::kBottom | LayoutSides::kTrailing);
    AddSameConstraintsToSides(
        self.contentView, _imageView,
        LayoutSides::kTop | LayoutSides::kBottom | LayoutSides::kLeading);
    [_imageView.trailingAnchor constraintEqualToAnchor:_title.leadingAnchor]
        .active = YES;
  }
  return self;
}

- (void)setTitleText:(NSString*)title {
  self.title.text = title;
}

@end
