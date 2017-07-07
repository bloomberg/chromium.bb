// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_COLLECTION_TAB_COLLECTION_TAB_CELL_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_COLLECTION_TAB_COLLECTION_TAB_CELL_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_panel_cell.h"

@class SnapshotCache;
@class TabCollectionItem;

// Cell represents a tab for use in the tab collection. It has a title, favicon,
// screenshot image, and close button. Cell selection is represented by a border
// highlight in the tintColor.
// PLACEHOLDER: Create custom implemementation rather than subclassing.
@interface TabCollectionTabCell : TabSwitcherLocalSessionCell
- (void)configureCell:(TabCollectionItem*)item
        snapshotCache:(SnapshotCache*)snapshotCache;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_COLLECTION_TAB_COLLECTION_TAB_CELL_H_
