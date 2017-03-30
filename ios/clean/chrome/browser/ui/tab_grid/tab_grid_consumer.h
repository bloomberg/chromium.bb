// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_CONSUMER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_CONSUMER_H_

#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_consumer.h"

// Interface to support addition or removal of an overlay.
@protocol TabGridConsumer<TabCollectionConsumer>

// Adds an overlay covering the entire tab grid that informs the user that
// there are no tabs.
- (void)addNoTabsOverlay;

// Removes the noTabsOverlay covering the entire tab grid.
- (void)removeNoTabsOverlay;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_CONSUMER_H_
