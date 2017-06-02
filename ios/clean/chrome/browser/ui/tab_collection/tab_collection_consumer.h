// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_COLLECTION_TAB_COLLECTION_CONSUMER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_COLLECTION_TAB_COLLECTION_CONSUMER_H_

@class TabCollectionItem;

// Interface to support insert/delete/updates to a tab collection.
@protocol TabCollectionConsumer

// Inserts |item| into tab collection at |index|. |SelectedIndex| is the index
// of the selected item in the tab collection.
- (void)insertItem:(TabCollectionItem*)item
           atIndex:(int)index
     selectedIndex:(int)selectedIndex;

// Deletes |index| from tab collection. |SelectedIndex| is the index
// of the selected tab in the tab collection.
- (void)deleteItemAtIndex:(int)index selectedIndex:(int)selectedIndex;

// Moves item from |fromIndex| to |toIndex|. |SelectedIndex| is the index
// of the selected tab in the tab collection.
- (void)moveItemFromIndex:(int)fromIndex
                  toIndex:(int)toIndex
            selectedIndex:(int)selectedIndex;

// Replaces item at |index| with |item|.
- (void)replaceItemAtIndex:(int)index withItem:(TabCollectionItem*)item;

// Selects the item at |selectedIndex|.
- (void)setSelectedIndex:(int)selectedIndex;

// Populates tab collection with |items|. |SelectedIndex| is the index
// of the selected item in the tab collection.
- (void)populateItems:(NSArray<TabCollectionItem*>*)items
        selectedIndex:(int)selectedIndex;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_COLLECTION_TAB_COLLECTION_CONSUMER_H_
