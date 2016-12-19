// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model_order_controller.h"

#include "base/logging.h"

@implementation TabModelOrderController

@synthesize insertionPolicy = insertionPolicy_;

- (id)initWithTabModel:(TabModel*)model {
  DCHECK(model);
  self = [super init];
  if (self) {
    model_ = model;
    insertionPolicy_ = TabModelOrderConstants::kInsertAfter;
  }
  return self;
}

- (NSUInteger)insertionIndexForTab:(Tab*)newTab
                        transition:(ui::PageTransition)transition
                            opener:(Tab*)parentTab
                         adjacency:(TabModelOrderConstants::InsertionAdjacency)
                                       adjacency {
  if (model_.isEmpty)
    return 0;

  if (PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK) &&
      parentTab) {
    NSUInteger referenceIndex = [model_ indexOfTab:parentTab];
    Tab* openLocation = nil;
    if (insertionPolicy_ == TabModelOrderConstants::kInsertAfter) {
      openLocation = [model_ lastTabWithOpener:parentTab];
    } else {
      openLocation = [model_ firstTabWithOpener:parentTab];
    }
    if (openLocation)
      referenceIndex = [model_ indexOfTab:openLocation];

    // If the reference tab is no longer in the model (e.g., it was closed
    // before the open request was handled), fall through to default behavior.
    if (referenceIndex != NSNotFound) {
      BOOL insertAfter =
          insertionPolicy_ == TabModelOrderConstants::kInsertAfter ||
          adjacency == TabModelOrderConstants::kAdjacentAfter;
      NSUInteger delta = insertAfter ? 1 : 0;
      return referenceIndex + delta;
    }
  }
  // In other cases, such as a normal new tab, open at the end.
  return [self insertionIndexForAppending];
}

- (NSUInteger)insertionIndexForAppending {
  return insertionPolicy_ == TabModelOrderConstants::kInsertAfter ? model_.count
                                                                  : 0;
}

- (Tab*)determineNewSelectedTabFromRemovedTab:(Tab*)removedTab {
  // While the desktop version of this code deals in indices, this deals in
  // actual tabs. As a result, the tab only needs to change iff the selection
  // is the tab that's removed.
  if (removedTab != model_.currentTab)
    return model_.currentTab;

  const NSUInteger numberOfTabs = model_.count;
  if (numberOfTabs < 2)
    return nil;
  DCHECK(numberOfTabs >= 2);
  DCHECK(removedTab == model_.currentTab);

  // First see if the tab being removed has any "child" tabs. If it does, we
  // want to select the first in that child group, not the next tab in the same
  // group of the removed tab.
  Tab* firstChild = [model_ nextTabWithOpener:removedTab afterTab:nil];
  if (firstChild)
    return firstChild;
  Tab* parentTab = [model_ openerOfTab:removedTab];
  if (parentTab) {
    // If the tab was in a group, shift selection to the next tab in the group.
    Tab* nextTab = [model_ nextTabWithOpener:parentTab afterTab:removedTab];
    if (nextTab)
      return nextTab;

    // If we can't find a subsequent group member, just fall back to the
    // parentTab itself. Note that we use "group" here since opener is
    // reset by select operations.
    return parentTab;
  }

  // No opener set, fall through to the default handler...
  NSUInteger selectedIndex = [model_ indexOfTab:removedTab];
  DCHECK(selectedIndex <= numberOfTabs - 1);
  // Is the closing tab the last one? If so, return the penultimate tab.
  if (selectedIndex == numberOfTabs - 1)
    return [model_ tabAtIndex:selectedIndex - 1];
  // Otherwise return the next tab after the current tab.
  return [model_ tabAtIndex:selectedIndex + 1];
}

@end
