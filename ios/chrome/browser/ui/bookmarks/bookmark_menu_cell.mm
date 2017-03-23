// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_menu_cell.h"

#import "ios/chrome/browser/ui/bookmarks/bookmark_menu_item.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Ink/src/MaterialInk.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
CGFloat kTrailingMargin = 16.0;
}  // namespace

@interface BookmarkMenuCell ()
// Set to YES if the cell can be selected.
@property(nonatomic, assign) BOOL isSelectable;
// Set to YES if the cell should animate on selection.
@property(nonatomic, assign) BOOL shouldAnimateHighlight;
// Icon view.
@property(nonatomic, weak) UIImageView* iconView;
// Text label.
@property(nonatomic, weak) UILabel* itemLabel;
// Separator view. Displayed at 1 pixel height on top for some cells.
@property(nonatomic, weak) UIView* separatorView;
@end

@implementation BookmarkMenuCell
@synthesize isSelectable = _isSelectable;
@synthesize shouldAnimateHighlight = _shouldAnimateHighlight;
@synthesize iconView = _iconView;
@synthesize itemLabel = _itemLabel;
@synthesize separatorView = _separatorView;
@synthesize inkView = _inkView;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    // Create icon view.
    UIImageView* iconView = [[UIImageView alloc] init];
    _iconView = iconView;

    // Create item label.
    UILabel* itemLabel = [[UILabel alloc] init];
    _itemLabel = itemLabel;

    self.contentView.layoutMargins = UIEdgeInsetsMakeDirected(
        0, bookmark_utils_ios::menuMargin, 0, kTrailingMargin);

    // Create stack view.
    UIStackView* contentStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[ iconView, itemLabel ]];
    [self.contentView addSubview:contentStack];

    contentStack.spacing = bookmark_utils_ios::titleToIconDistance;
    contentStack.alignment = UIStackViewAlignmentCenter;

    // Configure stack view layout.
    contentStack.translatesAutoresizingMaskIntoConstraints = NO;
    UIView* contentView = self.contentView;

    UILayoutGuide* marginsGuide = contentView.layoutMarginsGuide;
    [NSLayoutConstraint activateConstraints:@[
      [contentStack.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [contentStack.leadingAnchor
          constraintEqualToAnchor:marginsGuide.leadingAnchor],
      [contentStack.trailingAnchor
          constraintLessThanOrEqualToAnchor:marginsGuide.trailingAnchor]
    ]];

    // Create the ink view.
    _inkView = [[MDCInkView alloc] initWithFrame:self.bounds];
    [self addSubview:_inkView];

    // Add separator view.
    UIView* separatorView = [[UIView alloc] initWithFrame:CGRectZero];
    [self.contentView addSubview:separatorView];
    _separatorView = separatorView;

    CGFloat pixelSize = 1 / [[UIScreen mainScreen] scale];
    [NSLayoutConstraint activateConstraints:@[
      [_separatorView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor],
      [_separatorView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor],
      [_separatorView.heightAnchor constraintEqualToConstant:pixelSize],
      [_separatorView.topAnchor constraintEqualToAnchor:contentView.topAnchor],
    ]];
    _separatorView.translatesAutoresizingMaskIntoConstraints = NO;

    // Configure background colors.
    _separatorView.backgroundColor = [UIColor colorWithWhite:0.0 alpha:.12];
    self.backgroundColor = [UIColor clearColor];
  }
  return self;
}

- (void)prepareForReuse {
  self.shouldAnimateHighlight = NO;
  self.isSelectable = NO;
  [self.inkView cancelAllAnimationsAnimated:NO];
  [super prepareForReuse];
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated {
  [super setSelected:selected animated:animated];
  if (IsIPadIdiom())
    return;  // No background on selection on tablets.
  if (!self.isSelectable)
    return;
  if (selected)
    self.backgroundColor = [[MDCPalette greyPalette] tint100];
  else
    self.backgroundColor = [UIColor clearColor];
}

+ (BOOL)requiresConstraintBasedLayout {
  return YES;
}

- (void)updateWithBookmarkMenuItem:(BookmarkMenuItem*)item
                           primary:(BOOL)primary {
  self.itemLabel.text = [item titleForMenu];
  self.itemLabel.font = [MDCTypography body2Font];
  UIColor* textColor = nil;
  if (primary)
    textColor = [UIColor colorWithRed:66 / 255.0
                                green:129 / 255.0
                                 blue:224 / 255.0
                                alpha:1];
  else if (item.type == bookmarks::MenuItemSectionHeader)
    textColor = [UIColor colorWithWhite:0.0 alpha:0.54];
  else
    textColor = [UIColor blackColor];
  self.itemLabel.textColor = textColor;
  if (item.type == bookmarks::MenuItemDivider) {
    self.accessibilityElementsHidden = YES;
  } else {
    self.accessibilityIdentifier = [item accessibilityIdentifier];
    self.accessibilityLabel = [item titleForMenu];
    self.accessibilityTraits = [item accessibilityTraits];
  }
  if (primary)
    self.accessibilityTraits |= UIAccessibilityTraitSelected;
  else
    self.accessibilityTraits &= ~UIAccessibilityTraitSelected;
  self.iconView.image = [item imagePrimary:primary];
  self.separatorView.hidden = !(item.type == bookmarks::MenuItemDivider);

  self.isSelectable = (item.type != bookmarks::MenuItemSectionHeader) &&
                      (item.type != bookmarks::MenuItemDivider);
  self.shouldAnimateHighlight = !primary && self.isSelectable;

  // If there is no icon, hide the icon image view so that stack view redraws
  // without the icon, and vice versa.
  self.iconView.hidden = (self.iconView.image == nil);
}

+ (NSString*)reuseIdentifier {
  return @"BookmarkMenuCell";
}

@end
