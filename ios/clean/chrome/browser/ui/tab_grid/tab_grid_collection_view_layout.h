// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COLLECTION_VIEW_LAYOUT_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COLLECTION_VIEW_LAYOUT_H_

#import <UIKit/UIKit.h>

// Collection view layout that displays items in a grid flow layout with
// fixed columns and a vertical scroll. Items are square-shaped.
// Generally speaking, compact size classes have two columns, and regular size
// classes have more than two columns.
@interface TabGridCollectionViewLayout : UICollectionViewFlowLayout
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COLLECTION_VIEW_LAYOUT_H_
