// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SEARCH_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SEARCH_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_header_footer_item.h"

// Item for displaying a search.
@interface SettingsSearchItem : TableViewHeaderFooterItem

// The placeholder for the search input field.
@property(nonatomic, copy) NSString* placeholder;

// Whether or not the search field is enabled.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

@end

// Cell representation for SettingsSearchItem.
@interface SettingsSearchView : UITableViewHeaderFooterView

// Search bar of the cell.
@property(nonatomic, readonly, strong) UISearchBar* searchBar;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SEARCH_ITEM_H_
