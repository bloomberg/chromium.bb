// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_parent_folder_item.h"

#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarkParentFolderCell ()
@property(nonatomic, readwrite, strong) UILabel* parentFolderNameLabel;
@property(nonatomic, strong) UILabel* decorationLabel;
@end

@implementation BookmarkParentFolderItem

@synthesize title = _title;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.accessibilityIdentifier = @"Change Folder";
    self.cellClass = [BookmarkParentFolderCell class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(BookmarkParentFolderCell*)cell {
  [super configureCell:cell];
  cell.parentFolderNameLabel.text = self.title;
}

@end

@implementation BookmarkParentFolderCell

@synthesize parentFolderNameLabel = _parentFolderNameLabel;
@synthesize decorationLabel = _decorationLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (!self)
    return nil;

  self.isAccessibilityElement = YES;
  self.accessibilityTraits |= UIAccessibilityTraitButton;

  const CGFloat kHorizontalPadding = 15;
  const CGFloat kVerticalPadding = 8;
  const CGFloat kParentFolderLabelTopPadding = 7;

  _decorationLabel = [[UILabel alloc] init];
  _decorationLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _decorationLabel.text = l10n_util::GetNSString(IDS_IOS_BOOKMARK_GROUP_BUTTON);
  _decorationLabel.font =
      [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:12];
  _decorationLabel.textColor = bookmark_utils_ios::lightTextColor();
  [self.contentView addSubview:_decorationLabel];

  _parentFolderNameLabel = [[UILabel alloc] init];
  _parentFolderNameLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _parentFolderNameLabel.font =
      [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:16];
  _parentFolderNameLabel.textColor =
      [UIColor colorWithWhite:33.0 / 255.0 alpha:1.0];
  _parentFolderNameLabel.textAlignment = NSTextAlignmentNatural;
  [self.contentView addSubview:_parentFolderNameLabel];

  UIImageView* navigationChevronImage = [[UIImageView alloc] init];
  UIImage* image = TintImage([ChromeIcon chevronIcon], [UIColor grayColor]);
  navigationChevronImage.image = image;
  navigationChevronImage.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:navigationChevronImage];

  // Set up the constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_decorationLabel.topAnchor constraintEqualToAnchor:self.topAnchor
                                               constant:kVerticalPadding],
    [_decorationLabel.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                                   constant:kHorizontalPadding],
    [_parentFolderNameLabel.topAnchor
        constraintEqualToAnchor:_decorationLabel.bottomAnchor
                       constant:kParentFolderLabelTopPadding],
    [_parentFolderNameLabel.leadingAnchor
        constraintEqualToAnchor:_decorationLabel.leadingAnchor],
    [navigationChevronImage.centerYAnchor
        constraintEqualToAnchor:_parentFolderNameLabel.centerYAnchor],
    [navigationChevronImage.leadingAnchor
        constraintEqualToAnchor:_parentFolderNameLabel.trailingAnchor],
    [navigationChevronImage.widthAnchor
        constraintEqualToConstant:navigationChevronImage.image.size.width],
    [navigationChevronImage.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kHorizontalPadding],
  ]];

  self.shouldHideSeparator = YES;
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.parentFolderNameLabel.text = nil;
}

- (NSString*)accessibilityLabel {
  return self.parentFolderNameLabel.text;
}

- (NSString*)accessibilityHint {
  return l10n_util::GetNSString(
      IDS_IOS_BOOKMARK_EDIT_PARENT_FOLDER_BUTTON_HINT);
}

@end
