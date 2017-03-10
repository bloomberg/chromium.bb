// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_table_view_cell.h"

#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

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
  base::scoped_nsobject<BookmarkFolderTableViewCell> folderCell(
      [[[self class] alloc] initWithStyle:UITableViewCellStyleDefault
                          reuseIdentifier:[self folderCellReuseIdentifier]]);
  folderCell.get().indentationWidth = kFolderCellIndentationWidth;
  folderCell.get().imageView.image =
      [UIImage imageNamed:@"bookmark_gray_folder"];
  return folderCell.autorelease();
}

+ (instancetype)folderCreationCell {
  base::scoped_nsobject<BookmarkFolderTableViewCell> newFolderCell(
      [[[self class] alloc]
            initWithStyle:UITableViewCellStyleDefault
          reuseIdentifier:[self folderCreationCellReuseIdentifier]]);
  newFolderCell.get().textLabel.text =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_CREATE_GROUP);
  newFolderCell.get().imageView.image =
      [UIImage imageNamed:@"bookmark_gray_new_folder"];
  newFolderCell.get().accessibilityIdentifier = @"Create New Folder";
  return newFolderCell.autorelease();
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.textLabel.font = [MDCTypography subheadFont];
    self.textLabel.textColor = bookmark_utils_ios::darkTextColor();
    self.selectionStyle = UITableViewCellSelectionStyleGray;
    self.imageView.image = [UIImage imageNamed:@"bookmark_gray_folder"];
    self.accessibilityTraits |= UIAccessibilityTraitButton;
    _enabled = YES;
  }
  return self;
}

- (void)setChecked:(BOOL)checked {
  if (checked != _checked) {
    _checked = checked;
    base::scoped_nsobject<UIImageView> checkImageView(
        checked ? [[UIImageView alloc]
                      initWithImage:[UIImage imageNamed:@"bookmark_blue_check"]]
                : nil);
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

@end
