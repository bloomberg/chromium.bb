// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_FACADE_DELEGATE_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_FACADE_DELEGATE_H_

#include <stddef.h>

namespace content {
class NavigationController;
}

namespace web {

class NavigationManagerImpl;

// Interface used by the NavigationManager to drive the NavigationController
// facade. This pushes the ownership of the facade out of the web-layer to
// simplify upstreaming efforts.  Once upstream features are componentized and
// use NavigationManager, this class will no longer be necessary.
class NavigationManagerFacadeDelegate {
 public:
  NavigationManagerFacadeDelegate() {}
  virtual ~NavigationManagerFacadeDelegate() {}

  // Sets the NavigationManagerImpl that backs the NavigationController facade.
  virtual void SetNavigationManager(
      NavigationManagerImpl* navigation_manager) = 0;
  // Returns the facade object being driven by this delegate.
  virtual content::NavigationController* GetNavigationControllerFacade() = 0;

  // Callbacks for triggering notifications:

  // Called when the NavigationManager has a pending NavigationItem.
  virtual void OnNavigationItemPending() = 0;
  // Called when the NavigationManager has updated a NavigationItem.
  virtual void OnNavigationItemChanged() = 0;
  // Called when the NavigationManager has committed a pending NavigationItem.
  virtual void OnNavigationItemCommitted(int previous_item_index,
                                         bool is_in_page) = 0;
  // Called when the NavigationManager has pruned committed NavigationItems.
  virtual void OnNavigationItemsPruned(size_t pruned_item_count) = 0;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_MANAGER_FACADE_DELEGATE_H_
