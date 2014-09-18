// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_OBSERVER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_OBSERVER_H_

namespace history {
class TopSites;

// Interface for observing notifications from TopSites.
class TopSitesObserver {
 public:
  // Is called when TopSites finishes loading.
  virtual void TopSitesLoaded(history::TopSites* top_sites) = 0;

  // Is called when either one of the most visited urls
  // changed, or one of the images changes.
  virtual void TopSitesChanged(history::TopSites* top_sites) = 0;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_OBSERVER_H_
