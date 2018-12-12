// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_search_item.h"

#include "base/mac/foundation_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Margin for the search view.
const CGFloat kHorizontalMargin = 10.0f;
const CGFloat kVerticalMargin = 2.0f;
// Input field disabled alpha.
const CGFloat kDisabledAlpha = 0.6f;
}  // namespace

@implementation SettingsSearchItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsSearchView class];
    self.accessibilityTraits |= UIAccessibilityTraitSearchField;
    self.enabled = YES;
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureHeaderFooterView:(SettingsSearchView*)cell
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:cell withStyler:styler];
  cell.searchBar.placeholder = self.placeholder;
  cell.searchBar.userInteractionEnabled = self.enabled;
  cell.searchBar.alpha = self.enabled ? 1.0f : kDisabledAlpha;
}

@end

@implementation SettingsSearchView

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    UIView* contentView = self.contentView;

    _searchBar = [[UISearchBar alloc] initWithFrame:CGRectZero];
    _searchBar.translatesAutoresizingMaskIntoConstraints = NO;
    _searchBar.searchBarStyle = UISearchBarStyleMinimal;
    _searchBar.accessibilityIdentifier = @"SettingsSearchCellTextField";

    [contentView addSubview:_searchBar];
    // Set search bar fixed constraints for top, bottom and left side.
    AddSameConstraintsToSidesWithInsets(
        _searchBar, contentView,
        LayoutSides::kLeading | LayoutSides::kBottom | LayoutSides::kTop |
            LayoutSides::kTrailing,
        ChromeDirectionalEdgeInsetsMake(kVerticalMargin, kHorizontalMargin,
                                        kVerticalMargin, kHorizontalMargin));
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.searchBar.placeholder = @"";
  self.searchBar.userInteractionEnabled = YES;
  self.searchBar.alpha = 1.0f;
}

@end
