// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_

#include <map>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace prerender {

class PrerenderPendingSwapThrottle;

// Global object for maintaining prerender state on the IO thread.
class PrerenderTracker {
 public:
  typedef std::pair<int, int> ChildRouteIdPair;

  PrerenderTracker();
  virtual ~PrerenderTracker();

  // Returns whether or not a RenderFrame and URL are regarding a pending
  // prerender swap. Can only be called on the IO thread.
  bool IsPendingSwapRequestOnIOThread(int render_process_id,
                                      int render_frame_id,
                                      const GURL& url) const;

  // Called when a PrerenderPendingSwapThrottle defers a request. Cancel or
  // Resume will be called on |throttle| when the prerender is canceled or used,
  // respectively.
  void AddPendingSwapThrottleOnIOThread(
      int render_process_id, int render_frame_id, const GURL& url,
      const base::WeakPtr<PrerenderPendingSwapThrottle>& throttle);

  // Called to add throttles for a pending prerender swap.
  void AddPrerenderPendingSwap(
      const ChildRouteIdPair& render_frame_route_id_pair,
      const GURL& url);

  // Called to remove the throttles for a pending prerender swap.
  void RemovePrerenderPendingSwap(
      const ChildRouteIdPair& render_frame_route_id_pair,
      bool swap_successful);

 private:
  // Add/remove prerenders pending swap on the IO Thread.
  void AddPrerenderPendingSwapOnIOThread(
      const ChildRouteIdPair& render_frame_route_id_pair, const GURL& url);
  void RemovePrerenderPendingSwapOnIOThread(
      const ChildRouteIdPair& render_frame_route_id_pair,
      bool swap_successful);

  struct PendingSwapThrottleData {
    explicit PendingSwapThrottleData(const GURL& swap_url);
    ~PendingSwapThrottleData();
    GURL url;
    base::WeakPtr<PrerenderPendingSwapThrottle> throttle;
  };

  // Map of pending prerender swaps and their associated throttles,
  // maintained on the IO thread. The key is the routing ID pair
  // of a RenderFrame.
  typedef std::map<ChildRouteIdPair, PendingSwapThrottleData>
      PendingSwapThrottleMap;
  PendingSwapThrottleMap pending_swap_throttle_map_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderTracker);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_
