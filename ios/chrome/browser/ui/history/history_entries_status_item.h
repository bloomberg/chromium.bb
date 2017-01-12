// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_ENTRIES_STATUS_ITEM_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_ENTRIES_STATUS_ITEM_H_

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"

// Type of displayed history entries.  Entries can be synced or local, or there
// may be no history entries.
typedef NS_ENUM(NSInteger, HistoryEntriesStatus) {
  NO_ENTRIES,
  LOCAL_ENTRIES,
  SYNCED_ENTRIES
};

class GURL;
@class HistoryEntriesStatusItem;
@class LabelLinkController;

// Delegate HistoryEntriesStatusItem. Handles link taps on
// HistoryEntriesStatusCell.
@protocol HistoryEntriesStatusItemDelegate<NSObject>
// Called when a link is pressed on a HistoryEntriesStatusCell.
- (void)historyEntriesStatusItem:(HistoryEntriesStatusItem*)item
               didRequestOpenURL:(const GURL&)URL;

@end

// Model item for HistoryEntriesStatusCell. Manages links added to the cell.
@interface HistoryEntriesStatusItem : CollectionViewItem
// Status of currently displayed history entries.
@property(nonatomic, assign) HistoryEntriesStatus entriesStatus;
// YES if messages should be hidden.
@property(nonatomic, assign, getter=isHidden) BOOL hidden;
// YES if message for other forms of browsing data should be shown.
@property(nonatomic, assign) BOOL showsOtherBrowsingDataNotice;
// Delegate for HistoryEntriesStatusItem. Is notified when a link is pressed.
@property(nonatomic, weak) id<HistoryEntriesStatusItemDelegate> delegate;
@end

// Cell for displaying status for history entry. Provides information on whether
// local or synced entries or displays, and how to access other forms of
// browsing history, if applicable.
@interface HistoryEntriesStatusCell : CollectionViewFooterCell
@end

@interface HistoryEntriesStatusCell (Testing)
// Link controller for entries status message.
@property(nonatomic, retain, readonly) LabelLinkController* labelLinkController;
@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_ENTRIES_STATUS_ITEM_H_
