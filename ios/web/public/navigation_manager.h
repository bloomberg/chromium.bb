// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_MANAGER_H_
#define IOS_WEB_PUBLIC_NAVIGATION_MANAGER_H_

namespace web {

class BrowserState;
class NavigationItem;
class WebState;

// A NavigationManager maintains the back-forward list for a WebState and
// manages all navigation within that list.
//
// Each NavigationManager belongs to one WebState; each WebState has
// exactly one NavigationManager.
class NavigationManager {
 public:
  virtual ~NavigationManager() {}

  // Gets the BrowserState associated with this NavigationManager. Can never
  // return null.
  virtual BrowserState* GetBrowserState() const = 0;

  // Gets the WebState associated with this NavigationManager.
  virtual WebState* GetWebState() const = 0;

  // Returns the NavigationItem that should be used when displaying info about
  // the current entry to the user. It ignores certain pending entries, to
  // prevent spoofing attacks using slow-loading navigations.
  virtual NavigationItem* GetVisibleItem() const = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_MANAGER_H_
