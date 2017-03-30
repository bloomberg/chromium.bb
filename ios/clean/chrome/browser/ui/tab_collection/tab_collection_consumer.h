// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_COLLECTION_TAB_COLLECTION_CONSUMER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_COLLECTION_TAB_COLLECTION_CONSUMER_H_

// Interface to support insert/delete/updates to a tab collection.
@protocol TabCollectionConsumer

// Inserts item into tab collection at |index|.
- (void)insertItemAtIndex:(int)index;

// Deletes |index| from tab collection.
- (void)deleteItemAtIndex:(int)index;

// Reloads |indexes| in tab collection.
- (void)reloadItemsAtIndexes:(NSIndexSet*)indexes;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_COLLECTION_TAB_COLLECTION_CONSUMER_H_
