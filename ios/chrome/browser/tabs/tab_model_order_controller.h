// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_MODEL_ORDER_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TABS_TAB_MODEL_ORDER_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/tabs/tab_model.h"

// An object that allows different types of ordering and reselection to be
// heuristics plugged into a TabStripModel. Closely parallels
// chrome/browser/tabs/tab_strip_model_order_controller.h
// but without the dependence on TabContentsWrapper and TabStripModel.
@interface TabModelOrderController : NSObject

// Initializer, |model| must be non-nil and is not retained.
- (instancetype)initWithTabModel:(TabModel*)model;

// Returns the tab in which to shift selection after a tab is closed. May
// return nil if there are no more tabs.
- (Tab*)determineNewSelectedTabFromRemovedTab:(Tab*)removedTab;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_MODEL_ORDER_CONTROLLER_H_
