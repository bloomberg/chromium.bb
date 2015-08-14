// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_NAVIGATION_MANAGER_H_
#define IOS_WEB_PUBLIC_NAVIGATION_MANAGER_H_

#include "ios/web/public/browser_url_rewriter.h"

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

  // Returns the last committed NavigationItem, which may be null if there
  // are no committed entries.
  virtual NavigationItem* GetLastCommittedItem() const = 0;

  // Returns the pending entry corresponding to the navigation that is
  // currently in progress, or null if there is none.
  virtual NavigationItem* GetPendingItem() const = 0;

  // Removes the transient and pending NavigationItems.
  virtual void DiscardNonCommittedItems() = 0;

  // Adds |rewriter| to a transient list of URL rewriters.  Transient URL
  // rewriters will be executed before the rewriters already added to the
  // BrowserURLRewriter singleton, and the list will be cleared after the next
  // attempted page load.  |rewriter| must not be null.
  virtual void AddTransientURLRewriter(
      BrowserURLRewriter::URLRewriter rewriter) = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_NAVIGATION_MANAGER_H_
