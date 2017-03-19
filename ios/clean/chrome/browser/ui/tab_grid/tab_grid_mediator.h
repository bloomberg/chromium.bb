// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_data_source.h"

@protocol TabGridConsumer;
class WebStateList;

// Mediator listens for web state list changes, then updates the consumer.
// This also serves as data source for a tab grid.
@interface TabGridMediator : NSObject<TabGridDataSource>
// The source of changes and backing of the data source.
@property(nonatomic, assign) WebStateList* webStateList;
// This consumer is updated whenever there are relevant changes to the web state
// list.
@property(nonatomic, weak) id<TabGridConsumer> consumer;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_MEDIATOR_H_
