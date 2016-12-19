// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_MODEL_ORDER_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TABS_TAB_MODEL_ORDER_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/tabs/tab_model.h"
#include "ui/base/page_transition_types.h"

namespace TabModelOrderConstants {

// InsertionPolicy is a general policy for opening all new tabs.
enum InsertionPolicy {
  // Newly created tabs are created after the selection. This is the default.
  kInsertAfter,
  // Newly created tabs are inserted before the selection.
  kInsertBefore,
};

// InsertionAdjacency allows different links to choose to open tabs directly
// before or after a given tab, depending on context.
enum InsertionAdjacency {
  // Insert a card just before (to the left of) a given card.
  kAdjacentBefore,
  // Insert a card just after (to the right of) a given card.
  kAdjacentAfter,
};

}  // namespace TabModelOrderConstants

// An object that allows different types of ordering and reselection to be
// heuristics plugged into a TabStripModel. Closely parallels
// chrome/browser/tabs/tab_strip_model_order_controller.h
// but without the dependence on TabContentsWrapper and TabStripModel.
@interface TabModelOrderController : NSObject {
 @private
  TabModel* model_;  // weak, owns us
  TabModelOrderConstants::InsertionPolicy insertionPolicy_;
}

// Sets the insertion policy for new tabs. Default is |kInsertAfter|.
@property(nonatomic, assign)
    TabModelOrderConstants::InsertionPolicy insertionPolicy;

// Initializer, |model| must be non-nil and is not retained.
- (id)initWithTabModel:(TabModel*)model;

// Determines where to place a newly opened tab by using the transition and
// adjacency flags.
- (NSUInteger)insertionIndexForTab:(Tab*)newTab
                        transition:(ui::PageTransition)transition
                            opener:(Tab*)parentTab
                         adjacency:(TabModelOrderConstants::InsertionAdjacency)
                                       adjacency;

// Returns the index at which to append tabs, based on |insertionPolicy|.
- (NSUInteger)insertionIndexForAppending;

// Returns the tab in which to shift selection after a tab is closed. May
// return nil if there are no more tabs.
- (Tab*)determineNewSelectedTabFromRemovedTab:(Tab*)removedTab;

@end

#endif  // IOS_CHROME_BROWSER_TABS_TAB_MODEL_ORDER_CONTROLLER_H_
