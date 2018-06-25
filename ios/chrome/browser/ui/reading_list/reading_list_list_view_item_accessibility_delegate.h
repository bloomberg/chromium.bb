// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_VIEW_ITEM_ACCESSIBILITY_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_VIEW_ITEM_ACCESSIBILITY_DELEGATE_H_

@class ListItem;

@protocol ReadingListListViewItemAccessibilityDelegate

// Returns whether the entry is read.
- (BOOL)isEntryRead:(ListItem*)item;

- (void)deleteEntry:(ListItem*)item;
- (void)openEntryInNewTab:(ListItem*)item;
- (void)openEntryInNewIncognitoTab:(ListItem*)item;
- (void)openEntryOffline:(ListItem*)item;
- (void)markEntryRead:(ListItem*)item;
- (void)markEntryUnread:(ListItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_VIEW_ITEM_ACCESSIBILITY_DELEGATE_H_
