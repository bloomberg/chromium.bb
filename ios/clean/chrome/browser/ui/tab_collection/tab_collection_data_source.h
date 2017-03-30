// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_COLLECTION_TAB_COLLECTION_DATA_SOURCE_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_COLLECTION_TAB_COLLECTION_DATA_SOURCE_H_

#import <Foundation/Foundation.h>

const int kTabCollectionDataSourceInvalidIndex = -1;

// The data source for tab collection UI.
// Conceptually the tab collection represents a group of WebState objects (which
// are ultimately the model-layer representation of a browser tab). The data
// source must be able to map between indices and WebStates.
@protocol TabCollectionDataSource<NSObject>

// The number of tabs to be displayed in the collection.
- (int)numberOfTabs;

// Title for the tab at |index| in the collection.
- (NSString*)titleAtIndex:(int)index;

// Index for the active tab or kTabCollectionDataSourceInvalidIndex if there is
// no active tab.
- (int)indexOfActiveTab;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_COLLECTION_TAB_COLLECTION_DATA_SOURCE_H_
