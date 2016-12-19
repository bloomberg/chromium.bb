// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bars/bookmark_editing_bar.h"

#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_extended_button.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// The distance between buttons.
CGFloat kInterButtonMargin = 24;
}  // namespace

@interface BookmarkEditingBar () {
  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkEditingBar;
}
@property(nonatomic, retain) BookmarkExtendedButton* cancelButton;
// This label is slightly left off center, and reflects the number of bookmarks
// selected for editing.
@property(nonatomic, retain) UILabel* countLabel;
@property(nonatomic, retain) BookmarkExtendedButton* deleteButton;
@property(nonatomic, retain) UIView* dropShadow;
@property(nonatomic, retain) BookmarkExtendedButton* editButton;
@property(nonatomic, retain) BookmarkExtendedButton* moveButton;
@end

@implementation BookmarkEditingBar
@synthesize cancelButton = _cancelButton;
@synthesize countLabel = _countLabel;
@synthesize deleteButton = _deleteButton;
@synthesize dropShadow = _dropShadow;
@synthesize editButton = _editButton;
@synthesize moveButton = _moveButton;

- (id)initWithFrame:(CGRect)outerFrame {
  self = [super initWithFrame:outerFrame];
  if (self) {
    _propertyReleaser_BookmarkEditingBar.Init(self, [BookmarkEditingBar class]);
    self.backgroundColor = bookmark_utils_ios::blueColor();

    CGRect bounds = self.contentView.bounds;

    // Add the cancel button to the leading side of the bar.
    CGFloat cancelButtonWidth = 24;
    CGFloat cancelButtonHeight = 24;
    CGFloat cancelButtonY =
        floor((bounds.size.height - cancelButtonHeight) / 2);
    CGFloat cancelButtonX = cancelButtonY;
    base::scoped_nsobject<BookmarkExtendedButton> button(
        [[BookmarkExtendedButton alloc]
            initWithFrame:LayoutRectGetRect(LayoutRectMake(
                              cancelButtonX,
                              CGRectGetWidth(self.contentView.bounds),
                              cancelButtonY, cancelButtonWidth,
                              cancelButtonHeight))]);
    self.cancelButton = button;
    self.cancelButton.extendedEdges = UIEdgeInsetsMakeDirected(
        cancelButtonY, cancelButtonX, cancelButtonY, cancelButtonX);
    self.cancelButton.autoresizingMask =
        UIViewAutoresizingFlexibleTrailingMargin();
    self.cancelButton.backgroundColor = [UIColor clearColor];
    UIImage* cancelImage = [UIImage imageNamed:@"bookmark_white_close"];
    [self.cancelButton setBackgroundImage:cancelImage
                                 forState:UIControlStateNormal];
    self.cancelButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_CANCEL_BUTTON_LABEL);
    self.cancelButton.accessibilityIdentifier = @"Exit Edit Mode";
    [self.contentView addSubview:self.cancelButton];

    // Add the count label to the right of the cancel button.
    CGFloat labelX = bookmark_utils_ios::titleMargin;
    CGFloat labelY = 0;
    base::scoped_nsobject<UILabel> label([[UILabel alloc]
        initWithFrame:LayoutRectGetRect(LayoutRectMake(
                          labelX, CGRectGetWidth(self.contentView.bounds),
                          labelY, 150, CGRectGetHeight(bounds)))]);
    self.countLabel = label;
    self.countLabel.textColor = [UIColor whiteColor];
    self.countLabel.autoresizingMask =
        UIViewAutoresizingFlexibleTrailingMargin();
    self.countLabel.font = [MDCTypography titleFont];
    self.countLabel.backgroundColor = [UIColor clearColor];
    [self.contentView addSubview:self.countLabel];

    // Add the edit button to the right side of the bar.
    CGFloat editButtonWidth = 24;
    CGFloat editButtonHeight = 24;
    // The margin is the same to the top, the bottom, and the right.
    CGFloat editButtonY = floor((bounds.size.height - editButtonHeight) / 2);
    CGFloat editButtonRightMargin = editButtonY;
    CGFloat editButtonX =
        bounds.size.width - editButtonRightMargin - editButtonWidth;
    button.reset([[BookmarkExtendedButton alloc]
        initWithFrame:LayoutRectGetRect(LayoutRectMake(
                          editButtonX, CGRectGetWidth(self.contentView.bounds),
                          editButtonY, editButtonWidth, editButtonHeight))]);
    self.editButton = button;
    self.editButton.extendedEdges =
        UIEdgeInsetsMakeDirected(editButtonY, kInterButtonMargin / 2.0,
                                 editButtonY, editButtonRightMargin);
    self.editButton.autoresizingMask =
        UIViewAutoresizingFlexibleLeadingMargin();
    self.editButton.backgroundColor = [UIColor clearColor];
    UIImage* editImage = [UIImage imageNamed:@"bookmark_white_edit"];
    [self.editButton setBackgroundImage:editImage
                               forState:UIControlStateNormal];
    self.editButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_EDIT_BUTTON_LABEL);
    self.editButton.accessibilityIdentifier = @"Edit_editing_bar";
    [self.contentView addSubview:self.editButton];

    // Add the move button to the same position as the edit button.
    button.reset(
        [[BookmarkExtendedButton alloc] initWithFrame:self.editButton.frame]);
    self.moveButton = button;
    self.moveButton.extendedEdges =
        UIEdgeInsetsMakeDirected(editButtonY, kInterButtonMargin / 2.0,
                                 editButtonY, editButtonRightMargin);
    self.moveButton.autoresizingMask =
        UIViewAutoresizingFlexibleLeadingMargin();
    self.moveButton.backgroundColor = [UIColor clearColor];
    UIImage* moveImage = [UIImage imageNamed:@"bookmark_white_move"];
    [self.moveButton setBackgroundImage:moveImage
                               forState:UIControlStateNormal];
    self.moveButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_MOVE_BUTTON_LABEL);
    self.moveButton.accessibilityIdentifier = @"Move";
    [self.contentView addSubview:self.moveButton];

    // Add the delete button to the left of the edit button.
    CGFloat deleteButtonWidth = 24;
    CGFloat deleteButtonHeight = 24;
    CGFloat deleteButtonY =
        floor((bounds.size.height - deleteButtonHeight) / 2);
    CGFloat deleteButtonX =
        CGRectGetLeadingLayoutOffsetInBoundingRect(self.editButton.frame,
                                                   self.contentView.bounds) -
        kInterButtonMargin - deleteButtonWidth;
    button.reset([[BookmarkExtendedButton alloc]
        initWithFrame:LayoutRectGetRect(LayoutRectMake(
                          deleteButtonX,
                          CGRectGetWidth(self.contentView.bounds),
                          deleteButtonY, deleteButtonWidth,
                          deleteButtonHeight))]);
    self.deleteButton = button;
    self.deleteButton.extendedEdges = UIEdgeInsetsMakeDirected(
        deleteButtonY, deleteButtonY, deleteButtonY, kInterButtonMargin / 2.0);
    self.deleteButton.autoresizingMask =
        UIViewAutoresizingFlexibleLeadingMargin();
    self.deleteButton.backgroundColor = [UIColor clearColor];
    UIImage* deleteImage = [UIImage imageNamed:@"bookmark_white_delete"];
    [self.deleteButton setBackgroundImage:deleteImage
                                 forState:UIControlStateNormal];
    self.deleteButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_DELETE_BUTTON_LABEL);
    self.deleteButton.accessibilityIdentifier = @"Delete";
    [self.contentView addSubview:self.deleteButton];

    UIView* shadow =
        bookmark_utils_ios::dropShadowWithWidth(self.bounds.size.width);
    shadow.frame =
        CGRectMake(0, CGRectGetHeight(self.bounds),
                   CGRectGetWidth(shadow.frame), CGRectGetHeight(shadow.frame));
    [self addSubview:shadow];
    self.dropShadow = shadow;

    [self updateUIWithBookmarkCount:0 folderCount:0];
  }
  return self;
}

- (void)setCancelTarget:(id)target action:(SEL)action {
  [self.cancelButton addTarget:target
                        action:action
              forControlEvents:UIControlEventTouchUpInside];
}

- (void)setEditTarget:(id)target action:(SEL)action {
  [self.editButton addTarget:target
                      action:action
            forControlEvents:UIControlEventTouchUpInside];
}

- (void)setMoveTarget:(id)target action:(SEL)action {
  [self.moveButton addTarget:target
                      action:action
            forControlEvents:UIControlEventTouchUpInside];
}

- (void)setDeleteTarget:(id)target action:(SEL)action {
  [self.deleteButton addTarget:target
                        action:action
              forControlEvents:UIControlEventTouchUpInside];
}

- (void)updateUIWithBookmarkCount:(int)bookmarkCount
                      folderCount:(int)folderCount {
  DCHECK_GE(bookmarkCount, 0);
  DCHECK_GE(folderCount, 0);

  int editingCount = bookmarkCount + folderCount;

  if (editingCount == 0) {
    self.countLabel.text =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_ZERO_ITEM_LABEL);
  } else if (editingCount == 1) {
    self.countLabel.text =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_ONE_ITEM_LABEL);
  } else {
    NSString* editingCountString =
        [NSString stringWithFormat:@"%d", editingCount];
    self.countLabel.text =
        l10n_util::GetNSStringF(IDS_IOS_BOOKMARK_NEW_MANY_ITEM_LABEL,
                                base::SysNSStringToUTF16(editingCountString));
  }

  // Hide delete, move, edit buttons.
  if (editingCount == 0) {
    self.editButton.hidden = YES;
    self.moveButton.hidden = YES;
    self.deleteButton.hidden = YES;
    return;
  }

  // Hide move button. Show delete/edit buttons.
  if (editingCount == 1 && folderCount == 0) {
    self.editButton.hidden = NO;
    self.moveButton.hidden = YES;
    self.deleteButton.hidden = NO;
    return;
  }

  // Hide edit button. Show delete/move buttons.
  self.editButton.hidden = YES;
  self.moveButton.hidden = NO;
  self.deleteButton.hidden = NO;
}

- (void)showShadow:(BOOL)show {
  self.dropShadow.hidden = !show;
}

@end
