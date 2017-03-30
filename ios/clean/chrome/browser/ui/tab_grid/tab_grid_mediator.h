// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_MEDIATOR_H_

#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_mediator.h"

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_consumer.h"

// Mediator listens for web state list changes, then updates the consumer.
// This also serves as data source for a tab grid.
@interface TabGridMediator : TabCollectionMediator

// Redefined to subclass consumer.
@property(nonatomic, weak) id<TabGridConsumer> consumer;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_MEDIATOR_H_
