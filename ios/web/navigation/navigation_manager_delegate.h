// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_DELEGATE_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_DELEGATE_H_

#include <stddef.h>

#import "ios/web/public/navigation_manager.h"

@protocol CRWWebViewNavigationProxy;

namespace web {

struct LoadCommittedDetails;
class WebState;

// Delegate for NavigationManager to hand off parts of the navigation flow.
class NavigationManagerDelegate {
 public:
  virtual ~NavigationManagerDelegate() {}

  // Instructs the delegate to begin navigating to the item with index.
  // TODO(crbug.com/661316): Remove this method once all navigation code is
  // moved to NavigationManagerImpl.
  virtual void GoToIndex(int index) = 0;

  // Instructs the delegate to clear any transient content to prepare for new
  // navigation.
  virtual void ClearTransientContent() = 0;

  // Instructs the delegate to record page states (e.g. scroll position, form
  // values, whatever can be harvested) from the current page into the
  // navigation item.
  virtual void RecordPageStateInNavigationItem() = 0;

  // Instructs the delegate to notify its delegates that the current navigation
  // item will be loaded.
  virtual void WillLoadCurrentItemWithParams(
      const NavigationManager::WebLoadParams&,
      bool is_initial_navigation) = 0;

  // Instructs the delegate to load the current navigation item.
  virtual void LoadCurrentItem() = 0;

  // Instructs the delegate to reload.
  virtual void Reload() = 0;

  // Informs the delegate that committed navigation items have been pruned.
  virtual void OnNavigationItemsPruned(size_t pruned_item_count) = 0;

  // Informs the delegate that a navigation item has been changed.
  virtual void OnNavigationItemChanged() = 0;

  // Informs the delegate that a navigation item has been committed.
  virtual void OnNavigationItemCommitted(
      const LoadCommittedDetails& load_details) = 0;

  // Returns the WebState associated with this delegate.
  virtual WebState* GetWebState() = 0;

  // Returns a CRWWebViewNavigationProxy protocol that can be used to access
  // navigation related functions on the main WKWebView.
  virtual id<CRWWebViewNavigationProxy> GetWebViewNavigationProxy() const = 0;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_DELEGATE_H_
