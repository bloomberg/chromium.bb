// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_cell.h"

#include "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

namespace {
// Preferred image size in points.
const CGFloat kBookmarkTableCellDefaultImageSize = 16.0;

// Padding in table cell.
const CGFloat kBookmarkTableCellImagePadding = 16.0;
}  // namespace

@interface BookmarkTableCell ()

// The label, that displays placeholder text when favicon is missing.
@property(nonatomic, strong) UILabel* placeholderLabel;
@end

@implementation BookmarkTableCell
@synthesize placeholderLabel = _placeholderLabel;

#pragma mark - Initializer

- (instancetype)initWithReuseIdentifier:(NSString*)bookmarkCellIdentifier {
  self = [super initWithStyle:UITableViewCellStyleDefault
              reuseIdentifier:bookmarkCellIdentifier];
  if (self) {
    self.textLabel.font = [MDCTypography subheadFont];

    self.imageView.clipsToBounds = YES;
    [self.imageView setHidden:NO];

    _placeholderLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _placeholderLabel.textAlignment = NSTextAlignmentCenter;
    [_placeholderLabel setHidden:YES];
    [self.contentView addSubview:_placeholderLabel];
  }
  return self;
}

#pragma mark - Public

- (void)setNode:(const bookmarks::BookmarkNode*)node {
  self.textLabel.text = bookmark_utils_ios::TitleForBookmarkNode(node);
  self.textLabel.accessibilityIdentifier = self.textLabel.text;

  self.imageView.image = [UIImage imageNamed:@"bookmark_gray_folder"];
  if (node->is_folder()) {
    [self setAccessoryType:UITableViewCellAccessoryDisclosureIndicator];
  } else {
    [self setAccessoryType:UITableViewCellAccessoryNone];
  }
}

+ (NSString*)reuseIdentifier {
  return @"BookmarkTableCellIdentifier";
}

+ (CGFloat)preferredImageSize {
  return kBookmarkTableCellDefaultImageSize;
}

- (void)setImage:(UIImage*)image {
  [self.imageView setHidden:NO];
  [self.placeholderLabel setHidden:YES];

  [self.imageView setImage:image];
  [self setNeedsLayout];
}

- (void)setPlaceholderText:(NSString*)text
                 textColor:(UIColor*)textColor
           backgroundColor:(UIColor*)backgroundColor {
  [self.imageView setHidden:YES];
  [self.placeholderLabel setHidden:NO];

  self.placeholderLabel.backgroundColor = backgroundColor;
  self.placeholderLabel.textColor = textColor;
  self.placeholderLabel.text = text;
  [self.placeholderLabel sizeToFit];
  [self setNeedsLayout];
}

#pragma mark - Layout

- (void)prepareForReuse {
  self.imageView.image = nil;
  self.placeholderLabel.hidden = YES;
  self.imageView.hidden = NO;
  self.textLabel.text = nil;
  self.textLabel.accessibilityIdentifier = nil;
  [super prepareForReuse];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  CGFloat textLabelStart =
      kBookmarkTableCellDefaultImageSize + kBookmarkTableCellImagePadding * 2;
  // TODO(crbug.com/695749): Investigate why this separator inset is not
  // set correctly in landscape mode.
  [self setSeparatorInset:UIEdgeInsetsMake(0, textLabelStart, 0, 0)];

  // TODO(crbug.com/695749): Investigate using constraints instead of manual
  // layout.
  self.imageView.contentMode = UIViewContentModeScaleAspectFill;
  CGRect frame = CGRectMake(kBookmarkTableCellImagePadding, 0,
                            kBookmarkTableCellDefaultImageSize,
                            kBookmarkTableCellDefaultImageSize);
  self.imageView.frame = frame;
  self.imageView.center =
      CGPointMake(self.imageView.center.x, self.contentView.center.y);

  self.placeholderLabel.frame = frame;
  self.placeholderLabel.center =
      CGPointMake(self.placeholderLabel.center.x, self.contentView.center.y);

  CGRect textLabelFrame = self.textLabel.frame;
  self.textLabel.frame =
      CGRectMake(textLabelStart, textLabelFrame.origin.y,
                 textLabelFrame.size.width, textLabelFrame.size.height);
}

@end
