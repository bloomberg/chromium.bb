// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_folder_item.h"

#include "base/mac/foundation_util.h"
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

#pragma mark - BookmarkFolderItem

@interface BookmarkFolderItem ()
@property(nonatomic, assign) BookmarkFolderStyle style;
@end

@implementation BookmarkFolderItem
@synthesize currentFolder = _currentFolder;
@synthesize indentationLevel = _indentationLevel;
@synthesize style = _style;
@synthesize title = _title;

- (instancetype)initWithType:(NSInteger)type style:(BookmarkFolderStyle)style {
  if ((self = [super initWithType:type])) {
    self.cellClass = [LegacyTableViewBookmarkFolderCell class];
    self.style = style;
  }
  return self;
}

- (void)configureCell:(UITableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  LegacyTableViewBookmarkFolderCell* folderCell =
      base::mac::ObjCCastStrict<LegacyTableViewBookmarkFolderCell>(cell);

  switch (self.style) {
    case BookmarkFolderStyleNewFolder: {
      folderCell.textLabel.text =
          l10n_util::GetNSString(IDS_IOS_BOOKMARK_CREATE_GROUP);
      folderCell.imageView.image =
          [UIImage imageNamed:@"bookmark_gray_new_folder"];
      folderCell.accessibilityIdentifier = @"Create New Folder";
      break;
    }
    case BookmarkFolderStyleFolderEntry: {
      folderCell.textLabel.text = self.title;
      folderCell.accessibilityIdentifier = self.title;
      folderCell.accessibilityLabel = self.title;
      folderCell.checked = self.isCurrentFolder;
      folderCell.indentationLevel = self.indentationLevel;
      folderCell.indentationWidth = kFolderCellIndentationWidth;
      folderCell.imageView.image =
          [UIImage imageNamed:@"bookmark_gray_folder_new"];
      break;
    }
  }
}

@end

#pragma mark - LegacyTableViewBookmarkFolderCell

@implementation LegacyTableViewBookmarkFolderCell
@synthesize checked = _checked;
@synthesize enabled = _enabled;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.textLabel.font = [MDCTypography subheadFont];
    self.textLabel.textColor = bookmark_utils_ios::darkTextColor();
    self.selectionStyle = UITableViewCellSelectionStyleGray;
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
  self.indentationWidth = 0;
  self.imageView.image = nil;
}

@end
