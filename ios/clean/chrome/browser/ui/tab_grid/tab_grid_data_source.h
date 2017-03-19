// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_DATA_SOURCE_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_DATA_SOURCE_H_

#import <Foundation/Foundation.h>

const int kTabGridDataSourceInvalidIndex = -1;

// The data source for tab grid UI.
// Conceptually the tab grid represents a group of WebState objects (which
// are ultimately the model-layer representation of a browser tab). The data
// source must be able to map between indices and WebStates.
@protocol TabGridDataSource<NSObject>

// The number of tabs to be displayed in the grid.
- (int)numberOfTabsInTabGrid;

// Title for the tab at |index| in the grid.
- (NSString*)titleAtIndex:(int)index;

// Index for the active tab or kTabGridDataSourceInvalidIndex if there is no
// active tab.
- (int)indexOfActiveTab;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_DATA_SOURCE_H_
