// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_CONSUMER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_CONSUMER_H_

@protocol TabGridConsumer

// Inserts item into tab grid at |index|.
- (void)insertTabGridItemAtIndex:(int)index;

// Deletes |index| from tab grid.
- (void)deleteTabGridItemAtIndex:(int)index;

// Reloads |indexes| in tab grid.
- (void)reloadTabGridItemsAtIndexes:(NSIndexSet*)indexes;

// Adds an overlay covering the entire tab grid that informs the user that
// there are no tabs.
- (void)addNoTabsOverlay;

// Removes the noTabsOverlay covering the entire tab grid.
- (void)removeNoTabsOverlay;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_CONSUMER_H_
