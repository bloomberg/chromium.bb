// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SEARCH_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SEARCH_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// Delegate for SettingsSearchItem. Used to pass on search interactions.
@protocol SettingsSearchItemDelegate<NSObject>

// Called when the search term changes.
- (void)didRequestSearchForTerm:(NSString*)searchTerm;

@end

// View for displaying a search.
@interface SettingsSearchItem : CollectionViewItem

// Delegate for forwarding interactions with the search item.
@property(nonatomic, weak) id<SettingsSearchItemDelegate> delegate;

// The placeholder for the search input field.
@property(nonatomic, copy) NSString* placeholder;

// Whether or not the search field is enabled.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

@end

// Cell representation for SettingsSearchItem.
@interface SettingsSearchCell : MDCCollectionViewCell

// Text field for the search view.
@property(nonatomic, strong) UITextField* textField;

// Delegate for forwarding interactions with the search item.
@property(nonatomic, weak) id<SettingsSearchItemDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SEARCH_ITEM_H_
