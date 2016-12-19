// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tools_menu/reading_list_menu_view_item.h"

#include "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/reading_list/number_badge_view.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"

namespace {
// ID for cell reuse
static NSString* const kReadingListCellID = @"ReadingListCellID";
const CGFloat kToolsMenuItemTrailingMargin = 25;
}  // namespace

@interface ReadingListMenuViewCell () {
  base::scoped_nsobject<NumberBadgeView> _badge;
}
@end

@implementation ReadingListMenuViewItem

+ (NSString*)cellID {
  return kReadingListCellID;
}

+ (Class)cellClass {
  return [ReadingListMenuViewCell class];
}

@end

@implementation ReadingListMenuViewCell

- (void)initializeViews {
  if (_badge && [self title]) {
    return;
  }

  [super initializeViews];

  _badge.reset([[NumberBadgeView alloc] initWithFrame:CGRectZero]);
  [_badge setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self.contentView addSubview:_badge];

  [self.contentView removeConstraints:self.contentView.constraints];

  NSMutableArray<NSLayoutConstraint*>* constraintsToApply = [NSMutableArray
      arrayWithArray:[NSLayoutConstraint
                         constraintsWithVisualFormat:
                             @"H:|-(margin)-[title]-[badge]-(endMargin)-|"
                         options:NSLayoutFormatDirectionLeadingToTrailing
                         metrics:@{
                           @"margin" : @(self.horizontalMargin),
                           @"endMargin" : @(kToolsMenuItemTrailingMargin)
                         }
                         views:@{
                           @"title" : self.title,
                           @"badge" : _badge
                         }]];
  [constraintsToApply
      addObject:[self.title.centerYAnchor
                    constraintEqualToAnchor:self.contentView.centerYAnchor]];
  [constraintsToApply
      addObject:[_badge.get().centerYAnchor
                    constraintEqualToAnchor:self.contentView.centerYAnchor]];

  [NSLayoutConstraint activateConstraints:constraintsToApply];
}

- (void)updateBadgeCount:(NSInteger)count animated:(BOOL)animated {
  [_badge setNumber:count animated:animated];
}

- (void)updateSeenState:(BOOL)hasUnseenItems animated:(BOOL)animated {
  if (hasUnseenItems) {
    UIColor* highlightedColor = [[MDCPalette cr_bluePalette] tint500];

    [_badge setBackgroundColor:highlightedColor animated:animated];
    [self.title setTextColor:highlightedColor];
  } else {
    UIColor* regularColor = [[MDCPalette greyPalette] tint500];

    [_badge setBackgroundColor:regularColor animated:animated];
    [self.title setTextColor:[UIColor blackColor]];
  }
}

@end
