// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_COLLECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_COLLECTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"

#include "base/ios/block_types.h"

namespace ios {
class ChromeBrowserState;
}

@class HistoryCollectionViewController;
@protocol UrlLoader;

// Delegate for the history collection view controller.
@protocol HistoryCollectionViewControllerDelegate<NSObject>
// Notifies the delegate that history should be dismissed.
- (void)historyCollectionViewController:
            (HistoryCollectionViewController*)controller
              shouldCloseWithCompletion:(ProceduralBlock)completionHandler;
// Notifies the delegate that the collection view has scrolled to |offset|.
- (void)historyCollectionViewController:
            (HistoryCollectionViewController*)controller
                      didScrollToOffset:(CGPoint)offset;
// Notifies the delegate that history entries have been loaded or changed.
- (void)historyCollectionViewControllerDidChangeEntries:
    (HistoryCollectionViewController*)controller;
// Notifies the delegate that history entries have been selected or deselected.
- (void)historyCollectionViewControllerDidChangeEntrySelection:
    (HistoryCollectionViewController*)controller;
@end

// View controller for displaying a collection of history entries.
@interface HistoryCollectionViewController : CollectionViewController
// YES if the collection view is in editing mode. Setting |editing| turns
// editing mode on or off accordingly.
@property(nonatomic, assign, getter=isEditing) BOOL editing;
// YES if the the search bar is present.
@property(nonatomic, assign, getter=isSearching) BOOL searching;
// YES if collection is currently displaying any history entries.
@property(nonatomic, assign, readonly) BOOL hasHistoryEntries;
// YES if the collection view has selected entries while in editing mode.
@property(nonatomic, assign, readonly) BOOL hasSelectedEntries;

- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState
                      delegate:
                          (id<HistoryCollectionViewControllerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

// Search history for text |query| and display the results. |query| may be nil.
// If query is empty, show all history.
- (void)showHistoryMatchingQuery:(NSString*)query;

// Deletes selected items from browser history and removes them from the
// collection.
- (void)deleteSelectedItemsFromHistory;

@end

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_COLLECTION_VIEW_CONTROLLER_H_
