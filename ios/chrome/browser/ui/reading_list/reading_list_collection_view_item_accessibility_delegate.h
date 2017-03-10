// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_ITEM_ACCESSIBILITY_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_ITEM_ACCESSIBILITY_DELEGATE_H_

@class ReadingListCollectionViewItem;

@protocol ReadingListCollectionViewItemAccessibilityDelegate

// Returns whether the entry is read.
- (BOOL)isEntryRead:(ReadingListCollectionViewItem*)entry;

- (void)deleteEntry:(ReadingListCollectionViewItem*)entry;
- (void)openEntryInNewTab:(ReadingListCollectionViewItem*)entry;
- (void)openEntryInNewIncognitoTab:(ReadingListCollectionViewItem*)entry;
- (void)copyEntryURL:(ReadingListCollectionViewItem*)entry;
- (void)openEntryOffline:(ReadingListCollectionViewItem*)entry;
- (void)markEntryRead:(ReadingListCollectionViewItem*)entry;
- (void)markEntryUnread:(ReadingListCollectionViewItem*)entry;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COLLECTION_VIEW_ITEM_ACCESSIBILITY_DELEGATE_H_
