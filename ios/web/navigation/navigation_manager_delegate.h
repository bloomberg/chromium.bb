// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_DELEGATE_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_DELEGATE_H_

namespace web {

struct LoadCommittedDetails;
class WebState;

// Delegate for NavigationManager to hand off parts of the navigation flow.
// TODO(stuartmorgan): See if this can be eliminated by moving more
class NavigationManagerDelegate {
 public:
  virtual ~NavigationManagerDelegate() {}

  // Instructs the delegate to begin navigating to the pending entry.
  // TODO(stuartmorgan): Remove this once more navigation logic moves to
  // NavigationManagerImpl.
  virtual void NavigateToPendingEntry() = 0;

  // Informs the delegate that committed navigation items have been pruned.
  virtual void OnNavigationItemsPruned(size_t pruned_item_count) = 0;

  // Informs the delegate that a navigation item has been changed.
  virtual void OnNavigationItemChanged() = 0;

  // Informs the delegate that a navigation item has been commited.
  virtual void OnNavigationItemCommitted(
      const LoadCommittedDetails& load_details) = 0;

  // Returns the WebState associated with this delegate.
  virtual WebState* GetWebState() = 0;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_DELEGATE_H_
