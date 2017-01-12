// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_ENTRY_ITEM_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_ENTRY_ITEM_H_

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/third_party/material_components_ios/src/components/Collections/src/MaterialCollections.h"

namespace base {
class Time;
}  // namespace base

namespace history {
struct HistoryEntry;
}  // namespace history

namespace ios {
class ChromeBrowserState;
}  // namespace ios

@class FaviconView;
@protocol FaviconViewProviderDelegate;
class GURL;
@class HistoryEntryItem;

// Delegate for HistoryEntryItem. Handles actions invoked as custom
// accessibility actions.
@protocol HistoryEntryItemDelegate
// Called when custom accessibility action to delete the entry is invoked.
- (void)historyEntryItemDidRequestDelete:(HistoryEntryItem*)item;
// Called when custom accessibility action to open the entry in a new tab is
// invoked.
- (void)historyEntryItemDidRequestOpenInNewTab:(HistoryEntryItem*)item;
// Called when custom accessibility action to open the entry in a new incognito
// tab is invoked.
- (void)historyEntryItemDidRequestOpenInNewIncognitoTab:(HistoryEntryItem*)item;
// Called when custom accessibility action to copy the entry's URL is invoked.
- (void)historyEntryItemDidRequestCopy:(HistoryEntryItem*)item;
// Called when the view associated with the HistoryEntryItem should be updated.
- (void)historyEntryItemShouldUpdateView:(HistoryEntryItem*)item;
@end

// Model object for the cell that displays a history entry.
@interface HistoryEntryItem : CollectionViewItem

// Text for the content view. Rendered at the top trailing the favicon.
@property(nonatomic, copy) NSString* text;
// Detail text for content view. Rendered below text.
@property(nonatomic, copy) NSString* detailText;
// Text for the time stamp. Rendered aligned to trailing edge at same level as
// |text|.
@property(nonatomic, copy) NSString* timeText;
// URL of the associated history entry.
@property(nonatomic, assign) GURL URL;
// Timestamp of the associated history entry.
@property(nonatomic, assign) base::Time timestamp;

// The |delegate| is notified when the favicon has loaded, and may be nil.
- (instancetype)initWithType:(NSInteger)type
                historyEntry:(const history::HistoryEntry&)entry
                browserState:(ios::ChromeBrowserState*)browserState
                    delegate:(id<HistoryEntryItemDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

// HistoryEntryItems are equal if they have the same URL and
// timestamp.
- (BOOL)isEqualToHistoryEntryItem:(HistoryEntryItem*)item;

@end

// Cell that renders a history entry.
@interface HistoryEntryCell : MDCCollectionViewCell

// View for displaying the favicon for the history entry.
@property(nonatomic, strong) UIView* faviconViewContainer;
// Text label for history entry title.
@property(nonatomic, readonly, strong) UILabel* textLabel;
// Text label for history entry URL.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;
// Text label for history entry timestamp.
@property(nonatomic, readonly, strong) UILabel* timeLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_ENTRY_ITEM_H_
