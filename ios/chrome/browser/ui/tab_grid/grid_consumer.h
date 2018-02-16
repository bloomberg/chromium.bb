// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_CONSUMER_H_

@class GridItem;

// Supports insert/delete/updates to a grid.
@protocol GridConsumer

// Tells the consumer to replace its current set of items with |items| and
// update the selected index to be |selectedIndex|. It's an error to call this
// method with a |selectedIndex| value higher than the maximum index of |items|.
- (void)populateItems:(NSArray<GridItem*>*)items
        selectedIndex:(NSInteger)selectedIndex;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_CONSUMER_H_
