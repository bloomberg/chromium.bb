// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_table_view_cell.h"

#include "ios/chrome/browser/bookmarks/bookmark_new_generation_features.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The amount in points by which to offset horizontally the text label.
const CGFloat kTitleLabelLeadingOffset = 18.0;
// The amount in points by which to offset horizontally the image view.
const CGFloat kImageViewLeadingOffset = 1.0;
// Width by which to indent folder cell's content. This is multiplied by the
// |indentationLevel| of the cell.
const CGFloat kFolderCellIndentationWidth = 32.0;
}  // namespace

@implementation BookmarkFolderTableViewCell

@synthesize checked = _checked;
@synthesize enabled = _enabled;

+ (NSString*)folderCellReuseIdentifier {
  return @"BookmarkFolderCellReuseIdentifier";
}

+ (NSString*)folderCreationCellReuseIdentifier {
  return @"BookmarkFolderCreationCellReuseIdentifier";
}

+ (instancetype)folderCell {
  BookmarkFolderTableViewCell* folderCell =
      [[[self class] alloc] initWithStyle:UITableViewCellStyleDefault
                          reuseIdentifier:[self folderCellReuseIdentifier]];
  folderCell.indentationWidth = kFolderCellIndentationWidth;
  folderCell.imageView.image =
      [UIImage imageNamed:[[self class] bookmarkFolderImageName]];
  return folderCell;
}

+ (instancetype)folderCreationCell {
  BookmarkFolderTableViewCell* newFolderCell = [[[self class] alloc]
        initWithStyle:UITableViewCellStyleDefault
      reuseIdentifier:[self folderCreationCellReuseIdentifier]];
  newFolderCell.textLabel.text =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_CREATE_GROUP);
  newFolderCell.imageView.image =
      [UIImage imageNamed:@"bookmark_gray_new_folder"];
  newFolderCell.accessibilityIdentifier = @"Create New Folder";
  return newFolderCell;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.textLabel.font = [MDCTypography subheadFont];
    self.textLabel.textColor = bookmark_utils_ios::darkTextColor();
    self.selectionStyle = UITableViewCellSelectionStyleGray;
    self.imageView.image =
        [UIImage imageNamed:[[self class] bookmarkFolderImageName]];
    self.accessibilityTraits |= UIAccessibilityTraitButton;
    _enabled = YES;
  }
  return self;
}

- (void)setChecked:(BOOL)checked {
  if (checked != _checked) {
    _checked = checked;
    UIImageView* checkImageView =
        checked ? [[UIImageView alloc]
                      initWithImage:[UIImage imageNamed:@"bookmark_blue_check"]]
                : nil;
    self.accessoryView = checkImageView;
  }
}

- (void)setEnabled:(BOOL)enabled {
  if (enabled != _enabled) {
    _enabled = enabled;
    self.userInteractionEnabled = enabled;
    self.textLabel.enabled = enabled;
  }
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // Move the text label as required by the design.
  UIEdgeInsets insets =
      UIEdgeInsetsMakeDirected(0, kTitleLabelLeadingOffset, 0, 0);
  self.textLabel.frame = UIEdgeInsetsInsetRect(self.textLabel.frame, insets);

  // Indent the image. An offset is required in the design.
  LayoutRect layout = LayoutRectForRectInBoundingRect(self.imageView.frame,
                                                      self.contentView.bounds);
  layout.position.leading +=
      self.indentationWidth * self.indentationLevel + kImageViewLeadingOffset;
  self.imageView.frame = LayoutRectGetRect(layout);
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.checked = NO;
  self.enabled = YES;
}

#pragma mark - Private

// TODO(crbug.com/695749): Remove this function and use bookmark_gray_folder
// only when the new folder picker (and its folder cell class) is created for
// the new ui.
+ (NSString*)bookmarkFolderImageName {
  return (base::FeatureList::IsEnabled(kBookmarkNewGeneration))
             ? @"bookmark_gray_folder_new"
             : @"bookmark_gray_folder";
}

@end
