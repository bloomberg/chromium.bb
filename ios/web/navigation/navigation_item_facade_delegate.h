// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_ITEM_FACADE_DELEGATE_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_ITEM_FACADE_DELEGATE_H_

namespace content {
class NavigationEntry;
}

namespace web {

// Interface used by NavigationItems to interact with their NavigationEntry
// facades.  This pushes references to NavigationEntry out of the web-layer.
// Note that since communication is one-directional, this interface is primarily
// to add hooks for creation/deletion of facades.
class NavigationItemFacadeDelegate {
 public:
  NavigationItemFacadeDelegate() {}
  virtual ~NavigationItemFacadeDelegate() {}

  // Returns the facade object being driven by this delegate.
  virtual content::NavigationEntry* GetNavigationEntryFacade() = 0;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_ITEM_FACADE_DELEGATE_H_
